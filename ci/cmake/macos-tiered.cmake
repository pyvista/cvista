# cvista CI init-cache: macOS arm64 wheel, TIERED variant.
# Tier-purity module disables (cvista-config/tiered.cmake) layered over the normal
# macOS init-cache (Cocoa + arm64 + minimal.cmake). Select with
# CVISTA_CMAKE_INIT=ci/cmake/macos-tiered.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/../../cvista-config/tiered.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/macos.cmake")
