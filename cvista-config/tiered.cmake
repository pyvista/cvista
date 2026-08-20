# tiered.cmake — SINGLE-BUILD source for the 3-wheel split (core/rendering/io).
#
# Builds the union of the three tiers so partition_wheels.py can cut the built
# cvista/ tree into cvista (core) / cvista-rendering / cvista-io cleanly (proven by
# the script's DT_NEEDED self-containment audit).
#
# NOTE: the earlier CVISTA_CORE_CLEAN flag is GONE. It used to conditionally compile
# the rendering-coupled classes out of FiltersHybrid / IOGeometry. Those classes were
# since PHYSICALLY RELOCATED into their own modules — vtkFiltersHybridRendering (#167)
# and vtkIOImport (#168, the glTF reader/texture) — so FiltersHybrid and IOGeometry
# are now UNCONDITIONALLY rendering-free and land in the core tier with no flag.
#
# The parallel/CGNS/Infovis IO readers (IOParallel, IOParallelXML, IOCGNSReader,
# IOInfovis) used to be disabled here because they either mixed the core VTK::Parallel
# kit with io readers or were thought to pull rendering into the io tier. Those reasons
# are gone: #173 un-kitted IOParallel/IOParallelXML/IOCGNSReader (each is now a
# standalone lib, no VTK::Parallel kit membership -> no VTK::IO<->VTK::Parallel cycle),
# #168 made IOGeometry rendering-free, and IOInfovis only ever depended on InfovisCore
# under TEST_DEPENDS (its runtime DEPENDS are IOLegacy/IOXML). Their runtime deps now
# resolve entirely within {core, io} (ParallelCore is core; io -> core is legal), so
# they build with the WANT from _modules_minimal.cmake and partition_wheels.py routes
# them to the io tier. Nothing left to disable here.
#
# This file is a COMPOSABLE module-config layer, NOT a standalone init-cache: a per-OS
# tiered init-cache (ci/cmake/{linux,macos,windows}-tiered.cmake) includes THIS first,
# then the normal per-OS init-cache (Cocoa/MSVC/arch settings + minimal.cmake).
