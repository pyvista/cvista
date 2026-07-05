#!/usr/bin/env bash
# Prove the 3 tier wheels stack: core alone = IO-free offline compute; +io = every
# reader/writer; +rendering imports (rendering depends on io, installed underneath).
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

echo "=== 1) install CORE only (no deps) -> IO-free offline compute ==="
"$V/bin/pip" -q install --no-deps "$CORE"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
import cvista
from cvista.vtkFiltersCore import vtkAppendPolyData
from cvista.vtkFiltersSources import vtkSphereSource
# Pure in-memory pipeline: core carries NO file IO.
s = vtkSphereSource(); s.Update()
ap = vtkAppendPolyData(); ap.AddInputData(s.GetOutput()); ap.Update()
print("  core offline pipeline OK, points:", ap.GetOutput().GetNumberOfPoints())
# Every IO module (and rendering) must be absent from core.
for absent in ("vtkRenderingCore", "vtkIOCore", "vtkIOXML", "vtkIOLegacy",
               "vtkIOImage", "vtkIOGeometry", "vtkIOPLY", "vtkIOExodus"):
    try:
        __import__("cvista." + absent)
        print(f"  *** {absent} present (unexpected in core-only) ***"); raise SystemExit(1)
    except ImportError:
        print(f"  {absent}: absent [OK]")
PY

echo "=== 2) add IO tier -> every reader/writer works, core compute intact ==="
"$V/bin/pip" -q install --no-deps "$IO"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
import tempfile
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkIOXML import vtkXMLPolyDataWriter, vtkXMLPolyDataReader
from cvista.vtkIOExodus import vtkExodusIIReader
from cvista.vtkImagingHybridIO import vtkSliceCubes  # relocated out of ImagingHybrid
s = vtkSphereSource(); s.Update()
f = tempfile.mktemp(suffix=".vtp")
w = vtkXMLPolyDataWriter(); w.SetFileName(f); w.SetInputData(s.GetOutput()); w.Write()
r = vtkXMLPolyDataReader(); r.SetFileName(f); r.Update()
print("  io tier XML round-trip OK, points:", r.GetOutput().GetNumberOfPoints())
print("  vtkExodusIIReader:", type(vtkExodusIIReader()).__name__, "[io tier OK, stacked on core]")
print("  vtkSliceCubes (relocated):", type(vtkSliceCubes()).__name__, "[io tier OK]")
PY

echo "=== 3) add RENDERING tier -> imports (rendering depends on io underneath) ==="
"$V/bin/pip" -q install --no-deps "$REND"
LD_LIBRARY_PATH="$LDP" "$V/bin/python" - <<'PY'
import cvista.vtkRenderingCore as rc
print("  vtkRenderingCore import OK [rendering tier stacked]")
from cvista.vtkFiltersHybrid import vtkProcrustesAlignmentFilter
print("  core FiltersHybrid still importable after rendering add [OK]")
from cvista.vtkFiltersHybridRendering import vtkPolyDataSilhouette
print("  vtkPolyDataSilhouette (relocated) importable from rendering tier [OK]:", vtkPolyDataSilhouette().GetClassName())
# Rendering module that depends on io (scene export -> chemistry -> XML parser).
from cvista.vtkIOExport import vtkExporter
print("  vtkIOExport (rendering->io chain) importable [OK]")
PY
echo "=== STACKING VALIDATED ==="
