#!/usr/bin/env bash
#
# Run the bit-exact suite from INSIDE the manylinux2014 container: stock
# vtk==9.6.2 vs the fvtk container wheel (+ vtkmodules->fvtk shim). Both wheels are
# self-contained (auditwheel / Kitware manylinux), so NO LD_LIBRARY_PATH is needed.
# Asserts fvtk compute output is BYTE-FOR-BYTE identical (maxULP=0) to stock VTK.
#
# Usage: bash ci/bitexact_in_container.sh [WHEELDIR]   (default /wheels)
#
# Invoked as the command of `docker run -v <repo>:/src:ro -v <wheelhouse>:/wheels:ro`
# from an ordinary host runner, so actions/checkout/download-artifact (modern Node)
# stay on the host rather than the glibc-2.17 container where they cannot start.
set -euxo pipefail

WHEELDIR="${1:-/wheels}"
SRC="${SRC:-/src}"
PYBIN=/opt/python/cp313-cp313/bin

# --- stock VTK 9.6.2 backend (self-contained manylinux wheel) ---
"$PYBIN/python" -m venv /tmp/stock
/tmp/stock/bin/pip -q install --upgrade pip
/tmp/stock/bin/pip -q install "numpy==2.4.6" "vtk==9.6.2"
/tmp/stock/bin/python -c "import vtkmodules.vtkCommonCore as c; print('stock', c.vtkVersion.GetVTKVersion())"

# --- fvtk backend: the container wheel + vtkmodules->fvtk redirect shim ---
"$PYBIN/python" -m venv /tmp/fvtk
/tmp/fvtk/bin/pip -q install --upgrade pip
/tmp/fvtk/bin/pip -q install "numpy==2.4.6"
/tmp/fvtk/bin/pip -q install "$WHEELDIR"/*.whl
SP=$(/tmp/fvtk/bin/python -c 'import sysconfig;print(sysconfig.get_paths()["purelib"])')
cp "$SRC/tools/fvtk_shim.py" "$SP/_fvtk_shim.py"
echo "import _fvtk_shim" > "$SP/_fvtk_shim.pth"
/tmp/fvtk/bin/python -c "import vtkmodules.vtkCommonCore as c; print('fvtk', c.vtkVersion.GetVTKVersion())"

# --- runner (pytest) ---
"$PYBIN/python" -m venv /tmp/runner
/tmp/runner/bin/pip -q install --upgrade pip "numpy==2.4.6" pytest

# tests/ copied out of the read-only mount so pytest can write.
cp -r "$SRC/tests" /tmp/tests
cd /tmp/tests/bitexact
BITEXACT_STOCK_PY=/tmp/stock/bin/python \
BITEXACT_FVTK_PY=/tmp/fvtk/bin/python \
BITEXACT_STOCK_LDLP="" BITEXACT_FVTK_LDLP="" \
BITEXACT_OUTDIR="${BITEXACT_OUTDIR:-/tmp/bx-out}" \
/tmp/runner/bin/python -m pytest -v --tb=short -p no:cacheprovider
