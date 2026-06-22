#!/usr/bin/env bash
#
# Bit-exact gate: assert the built fvtk wheel is BYTE-FOR-BYTE identical to stock
# VTK 9.6.2 (maxULP=0). Runs on ANY host with a cp313 python — the wheels are
# self-contained manylinux/Kitware wheels, so no container is needed; only the
# build needs the old glibc. Two interpreters are compared (the suite dumps from
# each and diffs): a stock-vtk venv and an fvtk venv with the vtkmodules->fvtk shim.
#
#   Usage: ci/run-bitexact.sh <wheel-dir> [base-python]   (base-python default python3)
set -euxo pipefail

WHEELDIR="${1:?usage: ci/run-bitexact.sh <wheel-dir> [base-python]}"
BASE_PY="${2:-python3}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# stock VTK 9.6.2
"$BASE_PY" -m venv /tmp/stock
/tmp/stock/bin/pip -q install --upgrade pip
/tmp/stock/bin/pip -q install "numpy==2.4.6" "vtk==9.6.2"

# fvtk wheel + vtkmodules->fvtk redirect shim. WHEELDIR holds the single cp312-abi3
# wheel (Python 3.11 dropped); let pip pick the tag-compatible wheel via the local
# dir instead of globbing (robust if other artifacts are ever co-located).
# Install the freshly built wheel FIRST with --no-index: WHEELDIR holds a
# pre-release (.devN) wheel, and a bare `fvtk` requirement lets pip prefer a
# published PyPI release (e.g. 9.6.2.0) over it — silently testing the released
# version instead of this build. --no-index forces the local wheel (pip still
# tag-matches it); --no-deps defers deps. The second install then resolves
# fvtk's own deps (matplotlib/numpy/...) from PyPI; fvtk is already satisfied,
# so the published release is never pulled.
"$BASE_PY" -m venv /tmp/fvtk
/tmp/fvtk/bin/pip -q install --upgrade pip "numpy==2.4.6"
/tmp/fvtk/bin/pip -q install --no-index --no-deps --find-links "$WHEELDIR" fvtk
/tmp/fvtk/bin/pip -q install --find-links "$WHEELDIR" fvtk
SP=$(/tmp/fvtk/bin/python -c 'import sysconfig;print(sysconfig.get_paths()["purelib"])')
cp "$SRC/tools/fvtk_shim.py" "$SP/_fvtk_shim.py"
echo "import _fvtk_shim" > "$SP/_fvtk_shim.pth"

# runner (pytest)
"$BASE_PY" -m venv /tmp/runner
/tmp/runner/bin/pip -q install --upgrade pip "numpy==2.4.6" pytest

cd "$SRC/tests/bitexact"
# BITEXACT_ABI3 selects the parity mode: the shipped wheel is abi3 (heap types),
# so the gate defaults to abi3-aware (tolerates ONLY the type __flags__ HEAPTYPE/
# IMMUTABLETYPE divergence). Export BITEXACT_ABI3=0 before this script to validate
# a legacy static-type wheel (strict byte-for-byte parity incl. __flags__).
BITEXACT_STOCK_PY=/tmp/stock/bin/python \
BITEXACT_FVTK_PY=/tmp/fvtk/bin/python \
BITEXACT_ABI3="${BITEXACT_ABI3:-1}" \
BITEXACT_OUTDIR="${BITEXACT_OUTDIR:-/tmp/bx-out}" \
/tmp/runner/bin/python -m pytest -v --tb=short -p no:cacheprovider
