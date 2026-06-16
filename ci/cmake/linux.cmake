# fvtk CI init-cache: Linux x86_64 manylinux_2_17 wheel.
#
# Reuses fvtk-config/minimal.cmake (the deny-by-default PyVista closure +
# wheel-build knobs + the LTO / gold-ICF / hash-style / no-semantic-interposition
# levers). minimal.cmake already configures the Linux rendering stack we want:
# X/GLX + EGL + OSMesa with a non-headless default.
#
# Rendering deps (libGL/EGL/OSMesa/X11) come from the manylinux2014 container's
# el7 system packages installed in the workflow (yum). VTK discovers them on the
# default CMAKE_PREFIX_PATH. Validated configuring inside
# quay.io/pypa/manylinux2014_x86_64: glibc 2.17, devtoolset-10 (GCC 10.2.1),
# gold 2.35 with --icf=all, LTO plugin all present; all levers land in the cache.
#
# Note: this is the SAME init-cache PR #1 used for its 2_28 build; only the
# container image (and thus the auditwheel --plat) changes to reach 2_17.

include("${CMAKE_CURRENT_LIST_DIR}/../../fvtk-config/minimal.cmake")
