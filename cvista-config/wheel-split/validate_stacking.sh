#!/usr/bin/env bash
# Prove the 3 tier wheels stack: core alone = offline; +io = heavy readers; +rendering imports.
#
# Env:
#   WD       tier-wheels dir              (default /tmp/tier-wheels)
#   PYBIN    python to build the venv     (default python3, must be >=3.12 for abi3)
#   LDP      extra LD_LIBRARY_PATH        (optional; wheels resolve siblings via $ORIGIN)
set -euo pipefail
WD=${WD:-/tmp/tier-wheels}
PYBIN=${PYBIN:-python3}
LDP=${LDP:-}
CORE=$(ls "$WD"/cvista-9.6.2*.whl | head -1)
REND=$(ls "$WD"/cvista_rendering-9.6.2*.whl | head -1)
IO=$(ls "$WD"/cvista_io-9.6.2*.whl | head -1)
V=/tmp/venv-stack; rm -rf "$V"; "$PYBIN" -m venv "$V"

echo "=== 1) install CORE only (no deps) ==="
"$V/bin/pip" -q install --no-deps "$CORE"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
import cvista
from cvista.vtkFiltersCore import vtkAppendPolyData
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkIOXML import vtkXMLPolyDataWriter, vtkXMLPolyDataReader
s=vtkSphereSource(); s.Update()
import tempfile,os
f=tempfile.mktemp(suffix=".vtp")
w=vtkXMLPolyDataWriter(); w.SetFileName(f); w.SetInputData(s.GetOutput()); w.Write()
r=vtkXMLPolyDataReader(); r.SetFileName(f); r.Update()
print("  core offline pipeline OK, points:", r.GetOutput().GetNumberOfPoints())
for absent in ("vtkRenderingCore","vtkIOExodus"):
    try:
        __import__("cvista."+absent)
        print(f"  *** {absent} present (unexpected in core-only) ***"); raise SystemExit(1)
    except ImportError:
        print(f"  {absent}: absent [OK]")
PY

echo "=== 2) add IO tier -> heavy readers work ==="
"$V/bin/pip" -q install --no-deps "$IO"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
from cvista.vtkIOExodus import vtkExodusIIReader
from cvista.vtkIOXML import vtkXMLPolyDataReader   # core still works
print("  vtkExodusIIReader:", type(vtkExodusIIReader()).__name__, "[io tier OK, stacked on core]")
PY

echo "=== 3) add RENDERING tier -> import succeeds ==="
"$V/bin/pip" -q install --no-deps "$REND"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
import cvista.vtkRenderingCore as rc
print("  vtkRenderingCore import OK [rendering tier stacked]")
from cvista.vtkFiltersHybrid import vtkProcrustesAlignmentFilter
print("  core FiltersHybrid still importable after rendering add [OK]")
from cvista.vtkFiltersHybridRendering import vtkPolyDataSilhouette
print("  vtkPolyDataSilhouette (relocated) importable from rendering tier [OK]:", vtkPolyDataSilhouette().GetClassName())
PY
echo "=== STACKING VALIDATED ==="
