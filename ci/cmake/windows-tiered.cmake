# cvista CI init-cache: Windows x86_64 wheel, TIERED variant.
# Tier-purity module disables (cvista-config/tiered.cmake) layered over the normal
# Windows init-cache (MSVC + WGL + minimal.cmake). Select with
# CVISTA_CMAKE_INIT=ci/cmake/windows-tiered.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/../../cvista-config/tiered.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/windows.cmake")
