// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// VTK_DEPRECATED_IN_9_7_0()
#define VTK_DEPRECATION_LEVEL 0

#include "vtkDataArray.h"

#include "vtkAOSDataArrayTemplate.h"
#include "vtkArrayDispatch.h"
#include "vtkDataArrayRange.h"
#include "vtkGenericDataArray.h"
#include "vtkLookupTable.h"
#include "vtkSMPTools.h"
#include "vtkScaledSOADataArrayTemplate.h"

namespace
{

template <typename ValueType>
struct threadedCopyFunctor
{
  ValueType* src;
  ValueType* dst;
  int nComp;
  void operator()(vtkIdType begin, vtkIdType end) const
  {
    // std::copy(src+begin, src+end, dst+begin); //slower
    memcpy(dst + begin * nComp, src + begin * nComp, (end - begin) * nComp * sizeof(ValueType));
  }
};

// cvista: byte-wise twin of threadedCopyFunctor above, for the untemplated
// same-type AOS path in DeepCopy. Identical work, expressed in bytes so it does
// not need a ValueType and therefore costs no template instantiations.
struct threadedByteCopyFunctor
{
  char* src;
  char* dst;
  vtkIdType tupleBytes;
  void operator()(vtkIdType begin, vtkIdType end) const
  {
    memcpy(dst + begin * tupleBytes, src + begin * tupleBytes,
      static_cast<size_t>((end - begin) * tupleBytes));
  }
};

//--------Copy tuples from src to dest------------------------------------------
struct DeepCopyWorker
{
  // AoS --> AoS same-type specialization:
  template <typename ValueType>
  void operator()(
    vtkAOSDataArrayTemplate<ValueType>* src, vtkAOSDataArrayTemplate<ValueType>* dst) const
  {
    vtkIdType len = src->GetNumberOfTuples();
    if (len < 1024 * 1024)
    {
      // With less than a megabyte or so threading is likely to hurt performance. so don't
      std::copy(src->Begin(), src->End(), dst->Begin());
    }
    else
    {
      threadedCopyFunctor<ValueType> worker;
      worker.src = src->GetPointer(0);
      worker.dst = dst->GetPointer(0);
      worker.nComp = src->GetNumberOfComponents();
      // High granularity is likely to hurt performance too, so limit calls. 16 is about maximal.
      int numThreads = std::min(vtkSMPTools::GetEstimatedNumberOfThreads(), 16);
      vtkSMPTools::For(0, len, len / numThreads, worker);
    }
  }

#if defined(__clang__) && defined(__has_warning)
#if __has_warning("-Wunused-template")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-template"
#endif
#endif

  // SoA --> SoA same-type specialization:
  template <typename ValueType>
  void operator()(
    vtkSOADataArrayTemplate<ValueType>* src, vtkSOADataArrayTemplate<ValueType>* dst) const
  {
    dst->CopyData(src);
  }

  // ScaleSoA --> ScaleSoA same-type specialization:
  template <typename ValueType>
  void operator()(
    vtkScaledSOADataArrayTemplate<ValueType>* src, vtkScaledSOADataArrayTemplate<ValueType>* dst)
  {
    vtkIdType numTuples = src->GetNumberOfTuples();
    for (int comp = 0; comp < src->GetNumberOfComponents(); ++comp)
    {
      ValueType* srcBegin = src->GetComponentArrayPointer(comp);
      ValueType* srcEnd = srcBegin + numTuples;
      ValueType* dstBegin = dst->GetComponentArrayPointer(comp);

      std::copy(srcBegin, srcEnd, dstBegin);
    }
    dst->SetScale(src->GetScale());
  }

// Undo warning suppression.
#if defined(__clang__) && defined(__has_warning)
#if __has_warning("-Wunused-template")
#pragma clang diagnostic pop
#endif
#endif

  // Generic implementation:
  template <typename SrcArrayT, typename DstArrayT>
  void DoGenericCopy(SrcArrayT* src, DstArrayT* dst) const
  {
    const auto srcRange = vtk::DataArrayValueRange(src);
    auto dstRange = vtk::DataArrayValueRange(dst);

    using DstT = typename decltype(dstRange)::ValueType;
    auto destIter = dstRange.begin();
    // use for loop instead of copy to avoid -Wconversion warnings
    for (auto v = srcRange.cbegin(); v != srcRange.cend(); ++v, ++destIter)
    {
      *destIter = static_cast<DstT>(*v);
    }
  }

  // These overloads are split so that the above specializations will be
  // used properly.
  template <typename Array1DerivedT, typename Array1ValueT, int Array1ArrayType,
    typename Array2DerivedT, typename Array2ValueT, int Array2ArrayType>
  void operator()(vtkGenericDataArray<Array1DerivedT, Array1ValueT, Array1ArrayType>* src,
    vtkGenericDataArray<Array2DerivedT, Array2ValueT, Array2ArrayType>* dst) const
  {
    this->DoGenericCopy(src, dst);
  }

  void operator()(vtkDataArray* src, vtkDataArray* dst) const { this->DoGenericCopy(src, dst); }
};

} // end anon namespace

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
// Normally subclasses will do this when the input and output type of the
// DeepCopy are the same. When they are not the same, then we use the
// templated code below.
void vtkDataArray::DeepCopy(vtkDataArray* da)
{
  // Match the behavior of the old AttributeData
  if (da == nullptr)
  {
    return;
  }

  if (this != da)
  {
    this->Superclass::DeepCopy(da); // copy Information object

    vtkIdType numTuples = da->GetNumberOfTuples();
    int numComps = da->NumberOfComponents;

    this->SetNumberOfComponents(numComps);
    this->SetNumberOfTuples(numTuples);

    if (numTuples != 0)
    {
      // cvista: same-type AOS -> AOS is a straight byte copy, which is exactly
      // what DeepCopyWorker's AoS specialization below does. Doing it here,
      // untemplated, keeps that path for EVERY value type at zero instantiation
      // cost, because Dispatch2 only reaches the specialization for value types
      // that are in vtkArrayDispatch::Arrays. With the list trimmed
      // (CVISTA_DISPATCH_MINIMAL, see vtkCreateArrayDispatchArrayList.cmake) a
      // copy of e.g. the int32 connectivity array otherwise fell through to the
      // virtual per-tuple fallback and cost ~2.5 ns per element rather than
      // memcpy bandwidth: 7x to 75x slower than stock VTK for the six value
      // types off the list.
      //
      // Thresholds mirror the templated worker exactly so the types already on
      // the fast path keep their current behaviour, threading included.
      if (this->GetDataType() == da->GetDataType() &&
        this->GetArrayType() == vtkArrayTypes::VTK_AOS_DATA_ARRAY &&
        da->GetArrayType() == vtkArrayTypes::VTK_AOS_DATA_ARRAY)
      {
        const vtkIdType tupleBytes = static_cast<vtkIdType>(numComps) * this->GetDataTypeSize();
        char* src = static_cast<char*>(da->GetVoidPointer(0));
        char* dst = static_cast<char*>(this->GetVoidPointer(0));
        if (numTuples < 1024 * 1024)
        {
          memcpy(dst, src, static_cast<size_t>(numTuples * tupleBytes));
        }
        else
        {
          threadedByteCopyFunctor worker;
          worker.src = src;
          worker.dst = dst;
          worker.tupleBytes = tupleBytes;
          int numThreads = std::min(vtkSMPTools::GetEstimatedNumberOfThreads(), 16);
          vtkSMPTools::For(0, numTuples, numTuples / numThreads, worker);
        }
      }
      else
      {
        DeepCopyWorker worker;
        if (!vtkArrayDispatch::Dispatch2::Execute(da, this, worker))
        {
          // If dispatch fails, use fallback:
          worker(da, this);
        }
      }
    }

    this->SetLookupTable(nullptr);
    if (da->LookupTable)
    {
      this->LookupTable = da->LookupTable->NewInstance();
      this->LookupTable->DeepCopy(da->LookupTable);
    }
  }

  this->Squeeze();
}
VTK_ABI_NAMESPACE_END
