#!/usr/bin/env bash
#
# ci/run-cibuildwheel-linux.sh — drive cibuildwheel locally via docker for fvtk.
#
# Mirrors what the pypa/cibuildwheel GHA action does on CI, but on THIS box:
# cibuildwheel orchestrates the manylinux2014 container itself (mesa via
# CIBW_BEFORE_ALL, cmake build via the fvtk_backend, auditwheel repair to
# manylinux_2_17, smoke test under xvfb).
#
# Usage:
#   ci/run-cibuildwheel-linux.sh                       # default: full matrix
#   ci/run-cibuildwheel-linux.sh "cp311-* cp312-*"     # explicit selector
#   ci/run-cibuildwheel-linux.sh cp312-*               # just the abi3 leg
#
# The backend decides per leg: cp311 -> static cp311 wheel, cp312+ -> cp312-abi3
# wheel (cibuildwheel's abi3 dedup reuses it for cp313/cp314). The default
# selector is the full cp311..cp314 matrix, which yields TWO wheels (cp311 static
# + cp312 abi3). Set FVTK_ABI3=0 to force legacy static wheels on every leg.
#
# Requires a cibuildwheel on PATH (or in a venv): pip install cibuildwheel.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTDIR="${OUTDIR:-$REPO/wheelhouse-cibw}"
SELECTOR="${1:-cp311-* cp312-* cp313-* cp314-*}"

mkdir -p "$OUTDIR"

CIBW="${CIBW:-cibuildwheel}"
command -v "$CIBW" >/dev/null 2>&1 || CIBW="/tmp/cibw-venv/bin/cibuildwheel"

echo "=== cibuildwheel local (linux) ==="
echo "  selector : $SELECTOR"
echo "  outdir   : $OUTDIR"
echo "  ccache   : in-container /ccache (shared across the cp matrix within the run)"

# --only takes a single identifier; --build/CIBW_BUILD takes a glob. Use the env
# var so a multi-selector ("cp39-* cp313-*") works too.
CIBW_BUILD="$SELECTOR" \
  "$CIBW" --platform linux --output-dir "$OUTDIR" "$REPO"

echo "=== wheels ==="
ls -la "$OUTDIR"/*.whl
