# cvista CI init-cache: Linux x86_64 manylinux_2_28 wheel, TIERED variant.
#
# Identical to ci/cmake/linux.cmake except it sources cvista-config/tiered.cmake
# (which disables the kit-mixing niche IO readers, then includes minimal.cmake) so
# the resulting cvista/ tree partitions cleanly into the 3 tier wheels. Select it in
# CI with CVISTA_CMAKE_INIT=ci/cmake/linux-tiered.cmake; the default build keeps
# using ci/cmake/linux.cmake (the full monolithic wheel).
include("${CMAKE_CURRENT_LIST_DIR}/../../cvista-config/tiered.cmake")
