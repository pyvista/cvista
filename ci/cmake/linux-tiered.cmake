# cvista CI init-cache: Linux x86_64 manylinux_2_28 wheel, TIERED variant.
# Layers the tier-purity module disables (cvista-config/tiered.cmake) over the
# normal linux init-cache so the built cvista/ tree partitions cleanly into the 3
# tier wheels. Select with CVISTA_CMAKE_INIT=ci/cmake/linux-tiered.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/../../cvista-config/tiered.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/linux.cmake")
