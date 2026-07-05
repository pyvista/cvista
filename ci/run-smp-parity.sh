#!/usr/bin/env bash
#
# Build the standalone cvista_smp_parity validator (cvista-validate/) against an
# already-installed cvista via find_package(VTK), then run it. The validator
# exits nonzero if any algorithm's STDThread (parallel) output is not byte-exact
# with its Sequential (serial) output -- i.e. this is the CI gate on the
# "threaded == serial" claim in cvista-config/minimal.cmake.
#
# Expects ci/build-sdk.sh to have populated the install prefix first (default
# ./sdk-install). Mirrors build-sdk.sh's per-OS path/interpreter handling so it
# runs identically on Linux, macOS, and Windows.
#
#   Usage: INSTALL_PREFIX=./sdk-install ci/run-smp-parity.sh
set -euxo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${SRC}/sdk-install}"
BUILD_DIR="${SMP_BUILD_DIR:-${SRC}/build-smp-parity}"
REPEATS="${SMP_PARITY_REPEATS:-4}"

case "$(uname -s)" in
  Linux*)               CVISTA_OS=linux;   PYBIN="${PYBIN:-/opt/python/cp312-cp312/bin}" ;;
  Darwin*)              CVISTA_OS=macos;   PYBIN="${PYBIN:-$(dirname "$(command -v python3)")}" ;;
  MINGW*|MSYS*|CYGWIN*) CVISTA_OS=windows; PYBIN="${PYBIN:-$(dirname "$(command -v python)")}" ;;
  *) echo "run-smp-parity: unsupported OS $(uname -s)" >&2; exit 1 ;;
esac

# CMake/native tools want drive-letter paths on Windows; cygpath -m emits them.
cmpath() { if [ "$CVISTA_OS" = windows ]; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

export PATH="$PYBIN:$PATH" # pip-installed cmake/ninja from the build-sdk step

# Locate the installed VTK CMake package (…/lib/cmake/vtk-9.6/vtk-config.cmake).
VTK_DIR="${VTK_DIR:-$(dirname "$(find "$INSTALL_PREFIX" -name 'vtk-config.cmake' | head -1)")}"
if [ -z "$VTK_DIR" ] || [ ! -f "$VTK_DIR/vtk-config.cmake" ]; then
  echo "::error::run-smp-parity: no vtk-config.cmake under $INSTALL_PREFIX" >&2
  exit 1
fi
echo "using VTK_DIR=$VTK_DIR"

cmake -S "$(cmpath "$SRC/cvista-validate")" -B "$(cmpath "$BUILD_DIR")" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$(cmpath "$(command -v ninja)")" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVTK_DIR="$(cmpath "$VTK_DIR")"
cmake --build "$(cmpath "$BUILD_DIR")" --parallel

EXE="$BUILD_DIR/cvista_smp_parity"
[ "$CVISTA_OS" = windows ] && EXE="$BUILD_DIR/cvista_smp_parity.exe"

# The out-of-tree exe links the installed VTK shared libs; put them on the
# loader path (bin/ on Windows for the DLLs, lib/ elsewhere for the .so/.dylib).
case "$CVISTA_OS" in
  linux)   export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${LD_LIBRARY_PATH:-}" ;;
  macos)   export DYLD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${DYLD_LIBRARY_PATH:-}" ;;
  windows) export PATH="$(cygpath -u "$INSTALL_PREFIX/bin"):$PATH" ;;
esac

"$EXE" "$REPEATS"
