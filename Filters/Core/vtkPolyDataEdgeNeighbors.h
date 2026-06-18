// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file vtkPolyDataEdgeNeighbors.h
 * @brief Inline, devirtualized fast path for vtkPolyData::GetCellEdgeNeighbors.
 *
 * Several Filters/Core surface filters (vtkOrientPolyData, vtkFeatureEdges,
 * vtkDecimatePro, vtkSmoothPolyDataFilter, vtkSplitSharpEdgesPolyData) call
 * vtkPolyData::GetCellEdgeNeighbors once per polygon edge in their innermost
 * loop. That method lives in libvtkCommonDataModel, so each call from
 * libvtkFiltersCore pays a cross-.so PLT hop and re-fetches/re-types the
 * vtkPolyData::Links smart pointer on every edge.
 *
 * The body of GetCellEdgeNeighbors is purely integer bookkeeping over the cell
 * links (no floating point, no decision logic owned by the caller). This header
 * exposes the exact same algorithm as an inlinable helper so a filter can:
 *   1. resolve the typed cell-links pointer ONCE (outside its hot loop), and
 *   2. perform the edge-neighbor intersection inline at the call site,
 * eliminating the PLT hop and the per-call link re-fetch.
 *
 * BIT-EXACTNESS: the iteration order, the cellId exclusion, and the
 * InsertUniqueId de-duplication below are byte-for-byte identical to
 * GetCellEdgeNeighborsImpl in Common/DataModel/vtkPolyData.cxx. The de-dup uses
 * the inlined vtkIdList::IsId()/InsertNextId() pair (both inline in
 * vtkIdList.h), which is exactly what the canonical implementation does, so the
 * resulting list contents AND order match the public API for every input.
 */
#ifndef vtkPolyDataEdgeNeighbors_h
#define vtkPolyDataEdgeNeighbors_h

#include "vtkCellLinks.h"
#include "vtkIdList.h"
#include "vtkPolyData.h"
#include "vtkStaticCellLinks.h"

namespace vtkPolyDataEdgeNeighbors
{

// Core intersection. Templated on the links type (vtkStaticCellLinks or
// vtkCellLinks) so the GetNcells/GetCells accessors inline. This is an exact
// copy of GetCellEdgeNeighborsImpl in Common/DataModel/vtkPolyData.cxx.
template <class TLinks>
inline void GetCellEdgeNeighborsTyped(
  TLinks* links, vtkIdType cellId, vtkIdType p1, vtkIdType p2, vtkIdList* cellIds)
{
  const vtkIdType nCells1 = links->GetNcells(p1);
  const vtkIdType* cells1 = links->GetCells(p1);
  const vtkIdType nCells2 = links->GetNcells(p2);
  const vtkIdType* cells2 = links->GetCells(p2);

  for (vtkIdType i = 0; i < nCells1; ++i)
  {
    if (cells1[i] != cellId)
    {
      for (vtkIdType j = 0; j < nCells2; ++j)
      {
        if (cells1[i] == cells2[j])
        {
          // For degenerate cells, the same cells are linked several times to
          // the degenerate point, so a uniqueness check prevents duplicates.
          // IsId()/InsertNextId() are inline in vtkIdList.h; this reproduces the
          // canonical InsertUniqueId order/de-dup without an out-of-line call.
          if (cellIds->IsId(cells1[i]) < 0)
          {
            cellIds->InsertNextId(cells1[i]);
          }
          break;
        }
      }
    }
  }
}

/**
 * A reusable, devirtualized stand-in for vtkPolyData::GetCellEdgeNeighbors.
 *
 * Construct one of these from the vtkPolyData whose edge neighbors will be
 * queried (after BuildLinks() has been invoked). It resolves the typed links
 * pointer once; each Get() call then performs the intersection inline with no
 * cross-.so PLT hop and no per-call link re-fetch.
 *
 * If the links are neither vtkStaticCellLinks nor vtkCellLinks (an unexpected
 * custom links type), Get() falls back to the public API so behavior is always
 * identical to vtkPolyData::GetCellEdgeNeighbors.
 */
class FastEdgeNeighbors
{
public:
  explicit FastEdgeNeighbors(vtkPolyData* mesh)
    : Mesh(mesh)
  {
    vtkAbstractCellLinks* links = mesh->GetLinks();
    this->StaticLinks = vtkStaticCellLinks::SafeDownCast(links);
    if (!this->StaticLinks)
    {
      this->CellLinks = vtkCellLinks::SafeDownCast(links);
    }
  }

  // Equivalent to this->Mesh->GetCellEdgeNeighbors(cellId, p1, p2, cellIds),
  // including the leading cellIds->Reset().
  inline void Get(vtkIdType cellId, vtkIdType p1, vtkIdType p2, vtkIdList* cellIds) const
  {
    cellIds->Reset();
    if (this->StaticLinks)
    {
      GetCellEdgeNeighborsTyped<vtkStaticCellLinks>(
        this->StaticLinks, cellId, p1, p2, cellIds);
    }
    else if (this->CellLinks)
    {
      GetCellEdgeNeighborsTyped<vtkCellLinks>(this->CellLinks, cellId, p1, p2, cellIds);
    }
    else
    {
      // Reset already done; the public API resets again (harmless) and handles
      // any exotic links type or a not-yet-built links structure.
      this->Mesh->GetCellEdgeNeighbors(cellId, p1, p2, cellIds);
    }
  }

private:
  vtkPolyData* Mesh;
  vtkStaticCellLinks* StaticLinks = nullptr;
  vtkCellLinks* CellLinks = nullptr;
};

} // namespace vtkPolyDataEdgeNeighbors

#endif // vtkPolyDataEdgeNeighbors_h
// VTK-HeaderTest-Exclude: vtkPolyDataEdgeNeighbors.h
