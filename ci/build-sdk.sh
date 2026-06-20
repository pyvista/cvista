#!/usr/bin/env bash
#
# Build the fvtk install tree and the matching `fvtk-sdk` wheel.
#
# The SDK wheel ships the VTK C++ headers, CMake config, and wrap tools from the
# same source as the runtime `fvtk` wheel, wrapped as a scikit-build-core
# `cmake.prefix` package so a downstream `pip install fvtk-sdk` + `find_package(VTK)`
# just works. `CMake/vtkWheelPreparation.cmake` already configures
# <build>/wheel_sdks/pyproject.toml at configure time, pointing VTK_INSTALL_DIR at
# CMAKE_INSTALL_PREFIX; this script supplies that prefix, populates it with
# `cmake --install`, then builds the wheel from <build>/wheel_sdks.
#
# Runs inside quay.io/pypa/manylinux_2_28_x86_64 (same image + system deps as the
# runtime wheel build). The abi3 SDK is python-version-independent, so one wheel
# serves 3.12+.
#
#   Usage: ci/build-sdk.sh   (outputs wheel(s) to ./sdk-dist)
set -euxo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYBIN="${PYBIN:-/opt/python/cp312-cp312/bin}"
BUILD_DIR="${BUILD_DIR:-${SRC}/build-sdk}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/tmp/fvtk-install}"
OUT="${OUT:-${SRC}/sdk-dist}"

"$PYBIN/pip" install -U pip cmake ninja "setuptools_scm>=8"

# Put the pip cmake + ninja and the cpython on PATH so CMake's Ninja generator
# resolves CMAKE_MAKE_PROGRAM and the system compilers.
export PATH="$PYBIN:$PATH"

# Version suffix straight from the runtime wheel's backend so the SDK wheel
# version matches fvtk exactly (it applies the repo's setuptools_scm config:
# guess-next-dev, no-local-version, the 9.6.2 base and fallback).
SUFFIX="$("$PYBIN/python" -c "import sys; sys.path.insert(0, '$SRC/ci/cibw'); import fvtk_backend; print(fvtk_backend._version_suffix())")"

# LTO-off / -O2 fast config for the gate (the SDK content is headers + config +
# tools, not optimizer-sensitive); release uses the same script with FVTK_LTO
# unset for the shipped tools.
export FVTK_LTO="${FVTK_LTO:-0}"
export FVTK_GATE_O2="${FVTK_GATE_O2:-1}"
export CMAKE_C_COMPILER_LAUNCHER="${CMAKE_C_COMPILER_LAUNCHER:-ccache}"
export CMAKE_CXX_COMPILER_LAUNCHER="${CMAKE_CXX_COMPILER_LAUNCHER:-ccache}"

cmake -S "$SRC" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
    -C "$SRC/ci/cmake/linux.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DVTK_VERSION_SUFFIX="$SUFFIX"

cmake --build "$BUILD_DIR" --parallel "${FVTK_BUILD_JOBS:-$(nproc)}"
cmake --install "$BUILD_DIR"

# Build the fvtk-sdk wheel from the build-tree scaffold (pyproject.toml was
# configured there by vtkWheelPreparation with the install prefix baked in).
rm -rf "$OUT"
"$PYBIN/pip" wheel "$BUILD_DIR/wheel_sdks" --no-deps -w "$OUT"

ls -lh "$OUT"
