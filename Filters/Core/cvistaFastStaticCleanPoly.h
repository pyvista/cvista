// SPDX-License-Identifier: BSD-3-Clause
// cvista opt-in fast coincident-point merge for vtkStaticCleanPolyData.
//
// Thin VTK adapter over the vendored pyvista-algorithms OpenMP point-dedup
// kernel (pvaClean.h, MIT). Kept in a separate translation unit so the vendored
// code and its <omp.h> dependency are isolated from the rest of the
// (unity-built) FiltersCore module.
#ifndef cvistaFastStaticCleanPoly_h
#define cvistaFastStaticCleanPoly_h

#include "vtkABINamespace.h"

VTK_ABI_NAMESPACE_BEGIN
class vtkPolyData;
VTK_ABI_NAMESPACE_END

namespace cvista
{
VTK_ABI_NAMESPACE_BEGIN

/**
 * Opt-in fast coincident-point merge for a vtkPolyData (the vtkStaticCleanPolyData
 * fast path).
 *
 * No-op (returns false) unless cvista::FastModeEnabled() (env CVISTA_FAST /
 * cvista.EnableFast()) AND the request is the exact-merge default regime:
 *   - @p effectiveTolerance == 0 (exact coincident merge only),
 *   - @p averagePointData == false, @p produceMergeMap == false,
 *     @p hasMergingArray == false,
 *   - the input is POLYS-ONLY (no verts/lines/strips), no global point ids, no
 *     ghost points, float/double 3-component points, < 2^31 points,
 *   - and NO polygon collapses under the merge (a repeated id after remap would
 *     make stock dedup/convert the cell -- the kernel reports it and we fall back).
 * On success it fills @p output with the merged polydata (points deduped +
 * compacted, poly connectivity rewritten, polys kept 1:1 in input order so cell
 * data passes through unchanged) and returns true; the caller then skips the
 * standard path. The output is POINTS-relaxed: same merged point set and same
 * polys, but points are renumbered in a thread-/hash-dependent order.
 */
// Intra-module linkage (defined in cvistaFastStaticCleanPoly.cxx, called from
// vtkStaticCleanPolyData.cxx within the same shared library).
bool FastStaticCleanPolyData(vtkPolyData* input, vtkPolyData* output, double effectiveTolerance,
  bool removeUnusedPoints, bool averagePointData, bool produceMergeMap, bool hasMergingArray,
  int outputPointsPrecision);

VTK_ABI_NAMESPACE_END
} // namespace cvista

#endif
// VTK-HeaderTest-Exclude: cvistaFastStaticCleanPoly.h
