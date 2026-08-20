// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkDataArray.h"

#include "vtkArrayDispatch.h"
#include "vtkCVISTAByteCopy.h"
#include "vtkDataArrayRange.h"

namespace
{

struct CopyComponentWorker
{
  CopyComponentWorker(int srcComponent, int dstComponent)
    : SourceComponent(srcComponent)
    , DestinationComponent(dstComponent)
  {
  }

  template <typename ArraySrc, typename ArrayDst>
  void operator()(ArraySrc* dst, ArrayDst* src) const
  {
    const auto srcRange = vtk::DataArrayTupleRange(src);
    auto dstRange = vtk::DataArrayTupleRange(dst);

    using DstType = vtk::GetAPIType<ArrayDst>;
    auto dstIter = dstRange.begin();

    for (auto v : srcRange)
    {
      (*dstIter)[DestinationComponent] = static_cast<DstType>(v[SourceComponent]);
      ++dstIter;
    }
  }

  int SourceComponent = 0;
  int DestinationComponent = 0;
};

} // end anon namespace

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
bool vtkDataArray::CopyComponent(int dstComponent, vtkAbstractArray* source, int srcComponent)
{
  if (!source)
  {
    vtkErrorMacro(<< "The 'from' array must be non-null.");
    return false;
  }

  auto* src = vtkDataArray::SafeDownCast(source);
  if (!src)
  {
    vtkErrorMacro(<< "The 'from' array must be a vtkDataArray (not " << source->GetClassName()
                  << ").");
    return false;
  }

  return this->CopyComponent(dstComponent, src, srcComponent);
}

//------------------------------------------------------------------------------
bool vtkDataArray::CopyComponent(int dstComponent, vtkDataArray* src, int srcComponent)
{
  if (!src)
  {
    vtkErrorMacro(<< "The 'from' array must be non-null.");
    return false;
  }

  if (this->GetNumberOfTuples() != src->GetNumberOfTuples())
  {
    vtkErrorMacro(<< "Number of tuples in 'from' (" << src->GetNumberOfTuples() << ") and 'to' ("
                  << this->GetNumberOfTuples() << ") do not match.");
    return false;
  }

  if (dstComponent < 0 || dstComponent >= this->GetNumberOfComponents())
  {
    vtkErrorMacro(<< "Specified component " << dstComponent << " in 'to' array is not in [0, "
                  << this->GetNumberOfComponents() << ")");
    return false;
  }

  if (srcComponent < 0 || srcComponent >= src->GetNumberOfComponents())
  {
    vtkErrorMacro(<< "Specified component " << srcComponent << " in 'from' array is not in [0, "
                  << src->GetNumberOfComponents() << ")");
    return false;
  }

  if (cvista::SameTypeAOS(src, this))
  {
    cvista::CopyComponentBytes(src, srcComponent, this, dstComponent, this->GetNumberOfTuples());
    return true;
  }

  CopyComponentWorker copyComponentWorker(srcComponent, dstComponent);
  // cvista: the byte path above covers same-type AOS for every value type. What
  // is left is genuine cross-type conversion, which carries value-type semantics
  // and cannot be width-erased, so it keeps the dispatcher. Deleting it here cost
  // 5-7x on cross-type copies against stock, for ~67 KB of binary.
  if (!vtkArrayDispatch::Dispatch2::Execute(this, src, copyComponentWorker))
  {
    copyComponentWorker(this, src);
  }
  return true;
}
VTK_ABI_NAMESPACE_END
