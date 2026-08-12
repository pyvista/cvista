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
# Flat namespace: core classes resolve without naming the hosting module...
from cvista import vtkPolyData, vtkDecimatePro
print("  flat core resolve OK:", vtkPolyData().GetClassName(), vtkDecimatePro().GetClassName())
# ...and a missing tier's class raises the install hint, NOT a bare AttributeError.
for name, tier in (("vtkXMLPolyDataReader", "io"), ("vtkPolyDataMapper", "rendering")):
    try:
        getattr(cvista, name)
        print(f"  *** flat {name} resolved on core-only install ***"); raise SystemExit(1)
    except ImportError as e:
        assert f"cvista[{tier}]" in str(e), f"missing tier hint for {name}: {e}"
        print(f"  flat {name} -> install-hint ImportError naming {tier} tier [OK]")
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
# Flat namespace: the io class that raised on core-only now resolves.
from cvista import vtkXMLPolyDataReader as flat_reader
assert flat_reader is vtkXMLPolyDataReader
print("  flat vtkXMLPolyDataReader resolves after io install [OK]")
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
# Flat namespace: with all tiers stacked every indexed name must resolve --
# the strongest guarantee the flat API offers, checked exhaustively.
from cvista import _class_index, vtkPolyDataMapper
print("  flat vtkPolyDataMapper resolves after rendering install [OK]")
import cvista
bad = []
for _name in _class_index.INDEX:
    try:
        getattr(cvista, _name)
    except Exception as e:  # noqa: BLE001
        bad.append((_name, repr(e)))
if bad:
    print(f"  *** {len(bad)} indexed names failed to resolve, first 10: {bad[:10]} ***")
    raise SystemExit(1)
print(f"  flat index exhaustive resolve: all {len(_class_index.INDEX)} names [OK]")
PY
echo "=== STACKING VALIDATED ==="
