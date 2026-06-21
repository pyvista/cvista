#!/usr/bin/env bash
#
# Smoke-test the built fvtk-sdk wheel: install it and confirm a downstream
# scikit-build-core project resolves VTK via `find_package(VTK CONFIG)` through
# the `cmake.prefix` entry point. Exercises the wheel_sdks test suite copied into
# the build tree by vtkWheelPreparation.
#
#   Usage: ci/test-sdk.sh   (expects ci/build-sdk.sh to have produced ./sdk-dist)
set -euxo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYBIN="${PYBIN:-/opt/python/cp312-cp312/bin}"
BUILD_DIR="${BUILD_DIR:-${SRC}/build-sdk}"
OUT="${OUT:-${SRC}/sdk-dist}"

export PATH="$PYBIN:$PATH"

"$PYBIN/pip" install -U pip pytest virtualenv

# package-version parity check (importlib.metadata vs __version__)
"$PYBIN/pip" install --no-index --find-links "$OUT" fvtk-sdk
"$PYBIN/python" -m pytest "$BUILD_DIR/wheel_sdks/tests/test_package.py" -v

# find_package(VTK) integration: the test builds a downstream project that
# depends on fvtk-sdk; run it from the wheel dir so its `--find-links .` resolves
# the freshly built fvtk-sdk wheel.
cd "$OUT"
"$PYBIN/python" -m pytest "$BUILD_DIR/wheel_sdks/tests/test_find_package.py" -v
