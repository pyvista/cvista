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

# Version suffix, mirroring ci/cibw/fvtk_backend.py: everything past the base
# "9.6.2." (e.g. "post0.dev3"); empty on the exact 9.6.2 tag. The SDK wheel
# version then matches the runtime wheel so consumers pin them together.
SUFFIX="$("$PYBIN/python" - "$SRC" <<'PY'
import sys
from setuptools_scm import get_version
base = "9.6.2"
v = get_version(root=sys.argv[1], dist_name="fvtk")
print("" if v == base else v[len(base) + 1:])
PY
)"

# LTO-off / -O2 fast config for the gate (the SDK content is headers + config +
# tools, not optimizer-sensitive); release uses the same script with FVTK_LTO
# unset for the shipped tools.
export FVTK_LTO="${FVTK_LTO:-0}"
export FVTK_GATE_O2="${FVTK_GATE_O2:-1}"
export CMAKE_C_COMPILER_LAUNCHER="${CMAKE_C_COMPILER_LAUNCHER:-ccache}"
export CMAKE_CXX_COMPILER_LAUNCHER="${CMAKE_CXX_COMPILER_LAUNCHER:-ccache}"

"$PYBIN/python" -m cmake -S "$SRC" -B "$BUILD_DIR" -G Ninja \
    -C "$SRC/ci/cmake/linux.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DVTK_VERSION_SUFFIX="$SUFFIX"

"$PYBIN/python" -m cmake --build "$BUILD_DIR" --parallel "${FVTK_BUILD_JOBS:-$(nproc)}"
"$PYBIN/python" -m cmake --install "$BUILD_DIR"

# Build the fvtk-sdk wheel from the build-tree scaffold (pyproject.toml was
# configured there by vtkWheelPreparation with the install prefix baked in).
rm -rf "$OUT"
"$PYBIN/pip" wheel "$BUILD_DIR/wheel_sdks" --no-deps -w "$OUT"

ls -lh "$OUT"
