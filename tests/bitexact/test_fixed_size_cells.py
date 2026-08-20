"""Engagement gate for cvista's fixed-size cell-array outputs (issue #251).

``vtkCellArray`` supports fixed-size storage (implicit affine offsets instead of
a materialized offsets array). cvista makes filters whose output cell width is
known statically declare it up front, so the redundant offsets array is never
allocated. The connectivity *values* are unchanged, so this is transparent to
the bit-exact gate (a fixed-size affine offsets array materializes to the same
[0, k, 2k, ...] values); the main suite already proves parity for ``op_triangle``.

This test proves the optimization actually ENGAGED -- a property that is lost
when outputs are serialized to numpy for the compare gate, so it is checked at
runtime under the cvista python. ``vtkTriangleFilter`` emits uniform-width verts
(1), lines (2), polys (3) and decomposed strips (3); each output cell array must
report ``IsStorageFixedSize()``.

Only needs the cvista python (BITEXACT_CVISTA_PY); skips cleanly if unset.
"""

from __future__ import annotations

import os
import subprocess

import pytest

# Runs under the cvista python. Builds a polydata carrying all four cell types
# (a poly-vertex, a poly-line, a quad, and a triangle strip) so every output
# array of vtkTriangleFilter is exercised, then asserts each is fixed-size with
# the expected width -- and that the triangulated connectivity is still correct.
_SCRIPT = r"""
import os, sys
sys.path = [p for p in sys.path if p not in ("", os.getcwd())]
import numpy as np
from vtkmodules.util.numpy_support import vtk_to_numpy
from vtkmodules.vtkCommonCore import vtkPoints
from vtkmodules.vtkCommonDataModel import vtkPolyData, vtkCellArray
from vtkmodules.vtkFiltersCore import vtkTriangleFilter
from vtkmodules.vtkFiltersSources import vtkPlaneSource, vtkSphereSource

# --- 1. quad plane -> triangulate: the acceptance-criterion case -------------
plane = vtkPlaneSource(); plane.SetResolution(4, 4); plane.Update()
assert not plane.GetOutput().GetPolys().IsStorageFixedSize()  # width-4 input
tf = vtkTriangleFilter(); tf.SetInputData(plane.GetOutput()); tf.Update()
polys = tf.GetOutput().GetPolys()
assert polys.IsStorageFixedSize(), "plane.triangulate() polys not fixed-size"
conn = vtk_to_numpy(polys.GetConnectivityArray())
assert conn.size % 3 == 0 and polys.GetNumberOfCells() == conn.size // 3
# offsets materialize to the same affine [0,3,6,...] a stock offsets array holds
off = vtk_to_numpy(polys.GetOffsetsArray())
assert np.array_equal(off, np.arange(polys.GetNumberOfCells() + 1) * 3)

# --- 2. already-triangle input is passed through, storage preserved ----------
# When the input polys are already all triangles, the filter short-circuits and
# shallow/deep-copies the input cell array verbatim (it does not rebuild it), so
# the output storage is whatever the input carried -- the filter does not force
# fixed-size here. Assert only that the passthrough stays valid triangles.
sph = vtkSphereSource(); sph.SetThetaResolution(12); sph.SetPhiResolution(12); sph.Update()
tf2 = vtkTriangleFilter(); tf2.SetInputData(sph.GetOutput()); tf2.Update()
spolys = tf2.GetOutput().GetPolys()
sconn = vtk_to_numpy(spolys.GetConnectivityArray())
assert sconn.size % 3 == 0 and spolys.GetNumberOfCells() == sconn.size // 3

# --- 3. all four output arrays: verts(1)/lines(2)/polys(3)/strips->polys(3) ---
pts = vtkPoints()
for xyz in [(0,0,0),(1,0,0),(2,0,0),(0,1,0),(1,1,0),(2,1,0),(0,2,0),(1,2,0)]:
    pts.InsertNextPoint(*xyz)
pd = vtkPolyData(); pd.SetPoints(pts)

verts = vtkCellArray(); verts.InsertNextCell(3, [0, 1, 2])          # poly-vertex -> 3x width-1
lines = vtkCellArray(); lines.InsertNextCell(3, [0, 3, 6])          # poly-line   -> 2x width-2
polys = vtkCellArray(); polys.InsertNextCell(4, [0, 1, 4, 3])       # quad        -> 2x width-3
strips = vtkCellArray(); strips.InsertNextCell(4, [1, 4, 2, 5])     # strip       -> 2x width-3
pd.SetVerts(verts); pd.SetLines(lines); pd.SetPolys(polys); pd.SetStrips(strips)

tf3 = vtkTriangleFilter(); tf3.SetInputData(pd); tf3.Update()
out = tf3.GetOutput()
checks = {
    "verts": (out.GetVerts(), 1),
    "lines": (out.GetLines(), 2),
    "polys": (out.GetPolys(), 3),
}
for name, (ca, width) in checks.items():
    assert ca.GetNumberOfCells() > 0, name + " empty"
    assert ca.IsStorageFixedSize(), name + " not fixed-size"
    o = vtk_to_numpy(ca.GetOffsetsArray())
    assert np.array_equal(o, np.arange(ca.GetNumberOfCells() + 1) * width), name + " bad offsets"

print("OK")
"""


@pytest.fixture(scope="module")
def cvista_py():
    py = os.environ.get("BITEXACT_CVISTA_PY")
    if not py:
        pytest.skip("BITEXACT_CVISTA_PY not set; cannot check fixed-size engagement.")
    return py


def _env():
    env = dict(os.environ)
    ldlp = os.environ.get("BITEXACT_CVISTA_LDLP", "")
    if ldlp:
        existing = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = ldlp + (":" + existing if existing else "")
    return env


def test_triangle_filter_emits_fixed_size_cells(cvista_py, tmp_path):
    """vtkTriangleFilter's verts/lines/polys/strip outputs use fixed-size
    storage (no materialized offsets array), while connectivity is unchanged."""
    script = tmp_path / "fixed_size_check.py"
    script.write_text(_SCRIPT)
    proc = subprocess.run(
        [cvista_py, str(script)], env=_env(), capture_output=True, text=True
    )
    assert proc.returncode == 0, (
        f"fixed-size engagement check failed:\nSTDOUT:\n{proc.stdout}\n"
        f"STDERR:\n{proc.stderr}"
    )
    assert proc.stdout.strip().splitlines()[-1] == "OK"
