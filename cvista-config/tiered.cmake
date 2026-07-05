# tiered.cmake — SINGLE-BUILD source for the 3-wheel split (core/rendering/io).
#
# Builds the union of the three tiers, minus the few genuinely cross-cutting niche
# modules that would make a kit lib mix tiers, so partition_wheels.py can cut the
# built cvista/ tree into cvista (core) / cvista-rendering / cvista-io cleanly
# (proven by the script's DT_NEEDED self-containment audit).
#
# NOTE: the earlier CVISTA_CORE_CLEAN flag is GONE. It used to conditionally compile
# the rendering-coupled classes out of FiltersHybrid / IOGeometry. Those classes were
# since PHYSICALLY RELOCATED into their own modules — vtkFiltersHybridRendering (#167)
# and vtkIOImport (#168, the glTF reader/texture) — so FiltersHybrid and IOGeometry
# are now UNCONDITIONALLY rendering-free and land in the core tier with no flag.
#
# What remains: disable the kit-mixing / rendering-pulling niche IO readers so the
# core VTK::Parallel kit stays core-pure and the io tier stays rendering-free. These
# ship only in the monolithic full cvista wheel (built from minimal.cmake), not in
# the tier-split distribution:
#   - IOParallel / IOParallelXML : mix ParallelCore (core) with io-parallel readers in
#                                  the single VTK::Parallel kit lib.
#   - IOCGNSReader               : DEPENDS IOGeometry chain + rides the Parallel kit.
#   - IOInfovis                  : DEPENDS InfovisCore -> RenderingFreeType (pulls
#                                  rendering into what would be an io-tier module).
# This file is a COMPOSABLE module-disable layer, NOT a standalone init-cache. A
# per-OS tiered init-cache (ci/cmake/{linux,macos,windows}-tiered.cmake) includes
# THIS first (cache-first-wins over the WANT in _modules_minimal.cmake), then the
# normal per-OS init-cache (which carries the Cocoa/MSVC/arch settings + minimal.cmake).
foreach(_m IOParallel IOParallelXML IOCGNSReader IOInfovis)
  set(VTK_MODULE_ENABLE_VTK_${_m} NO CACHE STRING "")
endforeach()
