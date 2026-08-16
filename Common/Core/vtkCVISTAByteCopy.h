// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkCVISTAByteCopy
 * @brief   Type-erased fast paths for same-type array copies.
 *
 * This is a cvista (pyvista/cvista) addition, not part of upstream VTK.
 *
 * VTK's copy-shaped array operations -- DeepCopy, SetTuple, GetTuples,
 * InsertTuples, CopyComponent -- reach a memcpy through
 * `vtkArrayDispatch::Dispatch2`, which instantiates a worker for every ordered
 * pair of value types in `vtkArrayDispatch::Arrays`. That is N^2 template
 * instantiations to arrive at a byte move, and it only arrives there for value
 * types that are on the list. We trim that list hard
 * (`CVISTA_DISPATCH_MINIMAL`, see vtkCreateArrayDispatchArrayList.cmake), so
 * everything off it fell through to the virtual per-tuple path at a flat
 * ~2.5 ns/element: 7x to 75x slower than stock VTK (pyvista/cvista#255).
 *
 * When both arrays hold the same data type in contiguous AOS storage, the copy
 * does not need a value type at all -- element size is enough. Taking that path
 * here, untemplated, is both faster (memcpy bandwidth for EVERY value type, not
 * just the four on the list) and smaller (no instantiations at all), which is
 * why it is worth doing rather than widening the dispatch list.
 *
 * Anything else -- differing value types, SOA, implicit arrays -- is left to the
 * dispatcher untouched.
 */

#ifndef vtkCVISTAByteCopy_h
#define vtkCVISTAByteCopy_h

#include "vtkCommonCoreModule.h" // For export macro
#include "vtkDataArray.h"
#include "vtkType.h"

#include <cstring> // for memcpy

VTK_ABI_NAMESPACE_BEGIN

namespace cvista
{

/**
 * True when `src` and `dst` hold the same data type in contiguous AOS storage.
 * Component counts may differ, which is what CopyComponent needs.
 */
inline bool SameTypeAOS(vtkDataArray* src, vtkDataArray* dst)
{
  return src && dst && src->GetDataType() == dst->GetDataType() &&
    src->GetArrayType() == vtkArrayTypes::VTK_AOS_DATA_ARRAY &&
    dst->GetArrayType() == vtkArrayTypes::VTK_AOS_DATA_ARRAY;
}

/**
 * True when whole tuples can be copied as raw bytes: SameTypeAOS, and the tuples
 * are the same width.
 */
inline bool CanByteCopy(vtkDataArray* src, vtkDataArray* dst)
{
  return SameTypeAOS(src, dst) &&
    src->GetNumberOfComponents() == dst->GetNumberOfComponents();
}

/**
 * Bytes occupied by one tuple. Only meaningful alongside CanByteCopy.
 */
inline vtkIdType TupleBytes(vtkDataArray* array)
{
  return static_cast<vtkIdType>(array->GetNumberOfComponents()) * array->GetDataTypeSize();
}

/**
 * Copy `numTuples` whole tuples from `src[srcStart]` to `dst[dstStart]`.
 */
inline void CopyTuples(
  vtkDataArray* src, vtkIdType srcStart, vtkDataArray* dst, vtkIdType dstStart, vtkIdType numTuples)
{
  const vtkIdType tupleBytes = TupleBytes(src);
  memcpy(static_cast<char*>(dst->GetVoidPointer(0)) + dstStart * tupleBytes,
    static_cast<char*>(src->GetVoidPointer(0)) + srcStart * tupleBytes,
    static_cast<size_t>(numTuples * tupleBytes));
}

/**
 * Strided element move with the width known at compile time.
 *
 * A constant-size memcpy compiles to a single load/store pair, so this is a
 * tight strided loop rather than a call per element. Erasing by WIDTH instead of
 * by value type is what keeps this cheap: four instantiations cover every value
 * type VTK has, where dispatching on the value type would need one per type.
 */
template <int Width>
inline void StridedMove(const char* src, vtkIdType srcStride, char* dst, vtkIdType dstStride,
  vtkIdType count)
{
  for (vtkIdType i = 0; i < count; ++i)
  {
    memcpy(dst + i * dstStride, src + i * srcStride, Width);
  }
}

/**
 * Same, with the width chosen at runtime. Only the four scalar widths get a
 * specialised loop; anything wider (a multi-component tuple) is already large
 * enough that a plain memcpy call is not the bottleneck.
 */
inline void StridedMoveBytes(const char* src, vtkIdType srcStride, char* dst, vtkIdType dstStride,
  vtkIdType count, vtkIdType width)
{
  switch (width)
  {
    case 1:
      StridedMove<1>(src, srcStride, dst, dstStride, count);
      return;
    case 2:
      StridedMove<2>(src, srcStride, dst, dstStride, count);
      return;
    case 4:
      StridedMove<4>(src, srcStride, dst, dstStride, count);
      return;
    case 8:
      StridedMove<8>(src, srcStride, dst, dstStride, count);
      return;
    default:
      for (vtkIdType i = 0; i < count; ++i)
      {
        memcpy(dst + i * dstStride, src + i * srcStride, static_cast<size_t>(width));
      }
      return;
  }
}

/**
 * Indexed move, width known at compile time. `srcIds`/`dstIds` may each be null,
 * meaning "sequential", which covers gather and scatter with one kernel.
 */
template <int Width>
inline void IndexedMove(const char* src, const vtkIdType* srcIds, char* dst,
  const vtkIdType* dstIds, vtkIdType stride, vtkIdType count)
{
  for (vtkIdType i = 0; i < count; ++i)
  {
    const vtkIdType s = srcIds ? srcIds[i] : i;
    const vtkIdType d = dstIds ? dstIds[i] : i;
    memcpy(dst + d * stride, src + s * stride, Width);
  }
}

inline void IndexedMoveBytes(const char* src, const vtkIdType* srcIds, char* dst,
  const vtkIdType* dstIds, vtkIdType stride, vtkIdType count, vtkIdType width)
{
  switch (width)
  {
    case 1:
      IndexedMove<1>(src, srcIds, dst, dstIds, stride, count);
      return;
    case 2:
      IndexedMove<2>(src, srcIds, dst, dstIds, stride, count);
      return;
    case 4:
      IndexedMove<4>(src, srcIds, dst, dstIds, stride, count);
      return;
    case 8:
      IndexedMove<8>(src, srcIds, dst, dstIds, stride, count);
      return;
    default:
      for (vtkIdType i = 0; i < count; ++i)
      {
        const vtkIdType s = srcIds ? srcIds[i] : i;
        const vtkIdType d = dstIds ? dstIds[i] : i;
        memcpy(dst + d * stride, src + s * stride, static_cast<size_t>(width));
      }
      return;
  }
}

/**
 * Gather `numIds` tuples from `src[srcIds[i]]` into `dst[i]`.
 *
 * Pointers and the tuple width are hoisted out of the loop: doing this through
 * CopyTuples per id would repeat two virtual calls (GetVoidPointer,
 * GetDataTypeSize) for every tuple, which on a large id list costs more than
 * the copy.
 */
inline void GatherTuples(
  vtkDataArray* src, const vtkIdType* srcIds, vtkDataArray* dst, vtkIdType numIds)
{
  const vtkIdType tupleBytes = TupleBytes(src);
  IndexedMoveBytes(static_cast<char*>(src->GetVoidPointer(0)), srcIds,
    static_cast<char*>(dst->GetVoidPointer(0)), nullptr, tupleBytes, numIds, tupleBytes);
}

/**
 * Scatter `numIds` tuples from `src[srcIds[i]]` to `dst[dstIds[i]]`.
 */
inline void ScatterTuples(vtkDataArray* src, const vtkIdType* srcIds, vtkDataArray* dst,
  const vtkIdType* dstIds, vtkIdType numIds)
{
  const vtkIdType tupleBytes = TupleBytes(src);
  IndexedMoveBytes(static_cast<char*>(src->GetVoidPointer(0)), srcIds,
    static_cast<char*>(dst->GetVoidPointer(0)), dstIds, tupleBytes, numIds, tupleBytes);
}

/**
 * Copy one component across, tuple by tuple. Strided rather than contiguous, so
 * this is a loop of small memcpys; still branch-free per element and far ahead
 * of the virtual GetComponent/SetComponent pair it replaces.
 */
inline void CopyComponentBytes(
  vtkDataArray* src, int srcComponent, vtkDataArray* dst, int dstComponent, vtkIdType numTuples)
{
  const vtkIdType compBytes = src->GetDataTypeSize();
  StridedMoveBytes(static_cast<char*>(src->GetVoidPointer(0)) + srcComponent * compBytes,
    TupleBytes(src), static_cast<char*>(dst->GetVoidPointer(0)) + dstComponent * compBytes,
    TupleBytes(dst), numTuples, compBytes);
}

} // namespace cvista

VTK_ABI_NAMESPACE_END

#endif // vtkCVISTAByteCopy_h
// VTK-HeaderTest-Exclude: vtkCVISTAByteCopy.h
