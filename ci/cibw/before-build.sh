#!/usr/bin/env bash
# CIBW_BEFORE_BUILD (per cp leg): provide the cmake/ninja the backend needs.
#
# pip's cmake wheel is >=3.22 and pip's ninja is >=1.11 (el7's ninja-build 1.10
# errors "multiple outputs aren't (yet?) supported by depslog" on VTK's wrapping
# edges). cmake pinned <4.2 (4.1.x ok; VTK 9.6.2 declares ...4.0 compat).
set -euxo pipefail
# setuptools_scm (the backend's tag-driven version source) runs `git` against the
# mounted source tree; in the manylinux container the checkout is owned by a
# different uid, so git refuses with "dubious ownership" unless the path is marked
# safe. Without this the version derivation silently falls back to dev0 instead of
# the release tag. (macOS/Windows build natively, same uid, so they don't need it.)
git config --global --add safe.directory '*' || true
python -m pip install --upgrade pip
python -m pip install "cmake>=3.22,<4.2" "ninja>=1.11" "setuptools<81" wheel
cmake --version | sed -n 1p   # sed reads whole stream; head closes early -> SIGPIPE under pipefail
ninja --version

# oneTBB for the (default) TBB SMP backend. Built from source to a fixed prefix
# because there is NO uniform TBB dev package across our Linux images: AlmaLinux 8
# / el7 system TBB is classic-TBB 2018 (no oneTBB r1 ABI), and Intel's tbb-devel
# wheel has no aarch64/el7 build. Source build is small (~1-2 min, tests off) and
# gives one consistent oneTBB (r1 ABI, soname libtbb.so.12) on every arch/image.
# Guarded on the install marker so it runs ONCE per container (the cp matrix
# shares the container); /tmp is writable by the non-root build user and persists
# across legs. CMAKE_PREFIX_PATH=/tmp/tbb (find_package(TBB)) and
# LD_LIBRARY_PATH=/tmp/tbb/lib (auditwheel repair bundles libtbb) are set in
# [tool.cibuildwheel.linux.environment]. Set CVISTA_SMP_BACKEND=STDThread to skip.
TBB_PREFIX=/tmp/tbb
# CMAKE_INSTALL_LIBDIR=lib forces a deterministic layout: AlmaLinux's default
# GNUInstallDirs puts the config in lib64/cmake/TBB, but the build points TBB_DIR
# at $TBB_PREFIX/lib/cmake/TBB (CVISTA_TBB_DIR in [environment]), so pin lib.
if [ "${CVISTA_SMP_BACKEND:-}" = "TBB" ] && [ ! -e "$TBB_PREFIX/lib/cmake/TBB/TBBConfig.cmake" ]; then
  onetbb_src=/tmp/onetbb-src
  rm -rf "$onetbb_src"
  git clone --depth 1 -b v2022.0.0 https://github.com/uxlfoundation/oneTBB "$onetbb_src"
  cmake -S "$onetbb_src" -B "$onetbb_src/bld" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DTBB_TEST=OFF -DTBB_STRICT=OFF \
    -DCMAKE_INSTALL_PREFIX="$TBB_PREFIX" -DCMAKE_INSTALL_LIBDIR=lib
  cmake --build "$onetbb_src/bld" --target install
fi
ccache --zero-stats || true
