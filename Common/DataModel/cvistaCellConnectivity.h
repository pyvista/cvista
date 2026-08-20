// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file cvistaCellConnectivity.h
 * @brief Zero-copy, width-generic reader over a vtkCellArray's native connectivity.
 *
 * cvista defaults vtkCellArray storage to int32 while vtkIdType is int64. The
 * raw-pointer cell accessors that hand back point ids as `const vtkIdType*&`
 * (vtkPolyData::GetCellPoints, vtkCellArray::GetCellAtId, ...) can only alias the
 * connectivity buffer directly when its storage width equals vtkIdType. On an
 * int32 mesh they instead widen every cell into a shared scratch vtkIdList
 * (vtkAbstractCellArray::TempCell) on each call -- a per-cell copy plus a chain
 * of indirection (base hop -> virtual -> dispatch -> SetNumberOfIds + copy) that
 * stock VTK, built with 64-bit ids, never pays.
 *
 * This helper lets a filter read the same ids without that copy. It resolves the
 * native offsets/connectivity typed pointers ONCE (a single storage-type switch),
 * then reads a cell's point ids by indexing them directly; each id is widened to
 * vtkIdType only at the point of use (a value sign-extend), never materialized
 * into a buffer. No shared TempCell, no per-call virtual dispatch.
 *
 * BIT-EXACTNESS: the point ids returned are the exact integer VALUES stock VTK
 * returns -- only the container width differs, and int32 -> vtkIdType widening is
 * value-preserving. Handles the four AOS/fixed-size storage layouts; for any
 * other layout (Generic) IsValid() is false and the caller keeps the classic
 * accessor.
 *
 * The view holds raw pointers into the cell array's buffers, so it is valid only
 * while that vtkCellArray outlives it and its connectivity is not reallocated
 * (in-place edits such as ReplaceCellPointAtId are fine; inserting cells is not).
 * Ids are LOCAL to the cell array (for a vtkPolyData whose cells are all one type,
 * e.g. polys-only, the global cell id equals the local id).
 */
#ifndef cvistaCellConnectivity_h
#define cvistaCellConnectivity_h

#include "vtkCellArray.h" // for vtkCellArray and its int32/int64 array typedefs
#include "vtkType.h"      // for vtkTypeInt32/vtkTypeInt64/vtkIdType

VTK_ABI_NAMESPACE_BEGIN

class cvistaCellConnectivity
{
public:
  cvistaCellConnectivity() = default;
  explicit cvistaCellConnectivity(vtkCellArray* cells) { this->Capture(cells); }

  /// Resolve the native typed pointers for @a cells. Cheap; safe to re-call.
  void Capture(vtkCellArray* cells)
  {
    this->Conn32 = nullptr;
    this->Conn64 = nullptr;
    this->Off32 = nullptr;
    this->Off64 = nullptr;
    this->FixedSize = 0;
    if (!cells)
    {
      return;
    }
    switch (cells->GetStorageType())
    {
      case vtkCellArray::StorageTypes::Int32:
        this->Off32 = cells->GetOffsetsAOSArray32()->GetPointer(0);
        this->Conn32 = cells->GetConnectivityAOSArray32()->GetPointer(0);
        break;
      case vtkCellArray::StorageTypes::Int64:
        this->Off64 = cells->GetOffsetsAOSArray64()->GetPointer(0);
        this->Conn64 = cells->GetConnectivityAOSArray64()->GetPointer(0);
        break;
      case vtkCellArray::StorageTypes::FixedSizeInt32:
        this->Conn32 = cells->GetConnectivityAOSArray32()->GetPointer(0);
        this->FixedSize = cells->GetNumberOfCells() > 0 ? cells->GetCellSize(0) : 0;
        break;
      case vtkCellArray::StorageTypes::FixedSizeInt64:
        this->Conn64 = cells->GetConnectivityAOSArray64()->GetPointer(0);
        this->FixedSize = cells->GetNumberOfCells() > 0 ? cells->GetCellSize(0) : 0;
        break;
      default: // Generic and anything else: caller must fall back.
        break;
    }
  }

  /// True when native reads are available (AOS or fixed-size int32/int64 storage).
  bool IsValid() const { return this->Conn32 != nullptr || this->Conn64 != nullptr; }

  /// Index into the connectivity array at which cell @a cellId's points begin.
  vtkIdType CellBegin(vtkIdType cellId) const
  {
    if (this->FixedSize)
    {
      return cellId * this->FixedSize;
    }
    return this->Off64 ? static_cast<vtkIdType>(this->Off64[cellId])
                       : static_cast<vtkIdType>(this->Off32[cellId]);
  }

  /// Number of points in cell @a cellId.
  vtkIdType CellSize(vtkIdType cellId) const
  {
    if (this->FixedSize)
    {
      return this->FixedSize;
    }
    return this->Off64
      ? static_cast<vtkIdType>(this->Off64[cellId + 1] - this->Off64[cellId])
      : static_cast<vtkIdType>(this->Off32[cellId + 1] - this->Off32[cellId]);
  }

  /// Connectivity value at absolute index @a k (== CellBegin(cellId) + j), widened.
  vtkIdType operator[](vtkIdType k) const
  {
    return this->Conn64 ? static_cast<vtkIdType>(this->Conn64[k])
                        : static_cast<vtkIdType>(this->Conn32[k]);
  }

  /// Point @a j (0-based) of cell @a cellId, widened to vtkIdType.
  vtkIdType CellPoint(vtkIdType cellId, vtkIdType j) const
  {
    return (*this)[this->CellBegin(cellId) + j];
  }

private:
  const vtkTypeInt32* Conn32 = nullptr;
  const vtkTypeInt64* Conn64 = nullptr;
  const vtkTypeInt32* Off32 = nullptr;
  const vtkTypeInt64* Off64 = nullptr;
  vtkIdType FixedSize = 0; // > 0 => implicit affine offsets (fixed-size storage)
};

VTK_ABI_NAMESPACE_END

#endif // cvistaCellConnectivity_h
// VTK-HeaderTest-Exclude: cvistaCellConnectivity.h
