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

"$PYBIN/pip" install -U pip cmake ninja "setuptools_scm>=8" wheel twine

# Put the pip cmake + ninja and the cpython on PATH so CMake's Ninja generator
# resolves CMAKE_MAKE_PROGRAM and the system compilers.
export PATH="$PYBIN:$PATH"

# Version suffix straight from the runtime wheel's backend so the SDK wheel
# version matches fvtk exactly (it applies the repo's setuptools_scm config:
# guess-next-dev, no-local-version, the 9.6.2 base and fallback). _version_suffix()
# prints a diagnostic to stdout, so capture only the returned value.
SUFFIX="$("$PYBIN/python" -c "
import sys, io, contextlib
sys.path.insert(0, '$SRC/ci/cibw')
import fvtk_backend
with contextlib.redirect_stdout(io.StringIO()):
    s = fvtk_backend._version_suffix()
print(s)
")"

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

# scikit-build-core emits a raw `linux_x86_64` platform tag, which PyPI rejects
# (400 "unsupported platform tag"). This script runs inside
# quay.io/pypa/manylinux_2_28_x86_64, so manylinux_2_28 is the honest tag — just
# relabel it. We deliberately do NOT `auditwheel repair`: the SDK exposes the VTK
# shared/import libs unvendored so downstream `find_package(VTK)` links them by
# their real SONAMEs, and repair would rewrite those with hashed names and break
# that contract. (The py3-none half of the tag comes from wheel.py-api in
# CMake/wheel_sdks/pyproject.toml.in.)
WHEEL="$(ls "$OUT"/fvtk_sdk-*-linux_x86_64.whl)"
"$PYBIN/python" -m wheel tags --platform-tag manylinux_2_28_x86_64 --remove "$WHEEL"

# Fail loudly if any wheel still carries a PyPI-rejected raw linux tag, then run
# twine's metadata/tag validation as the publish job will.
if compgen -G "$OUT/*-linux_x86_64.whl" >/dev/null; then
    echo "::error::fvtk-sdk wheel still has a raw linux_x86_64 platform tag; PyPI will 400 on upload"
    exit 1
fi
"$PYBIN/python" -m twine check "$OUT"/*.whl

ls -lh "$OUT"
