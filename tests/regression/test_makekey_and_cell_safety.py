"""Regression tests for MakeKey lifetime (#249), Windows SDK DLL naming (#299),
and cell/connectivity thread and memory safety (#223, #221, #229).
"""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import gc
from pathlib import Path
import subprocess
import sys
import threading
from unittest.mock import patch

import numpy as np
import pytest

import cvista
from cvista.util import keys


def test_makekey_survives_python_gc_and_lookup():
    """Keys created via MakeKey are owned by the global key manager (#249).

    Dropping the Python wrapper must not delete the underlying C++ singleton
    while a vtkInformation instance still references it.
    """
    info = cvista.vtkInformation()
    k_str = keys.MakeKey(keys.StringKey, "reg_str", "cvista.test")
    k_int = keys.MakeKey(keys.IntegerKey, "reg_int", "cvista.test")
    k_dbl = keys.MakeKey(keys.DoubleVectorKey, "reg_dbl_vec", "cvista.test", 3)

    info.Set(k_str, "value")
    info.Set(k_int, 42)
    info.Set(k_dbl, (1.0, 2.0, 3.0), 3)

    del k_str, k_int, k_dbl
    gc.collect()

    it = cvista.vtkInformationIterator()
    it.SetInformation(info)
    it.InitTraversal()
    recovered = {}
    while not it.IsDoneWithTraversal():
        key = it.GetCurrentKey()
        recovered[(key.GetName(), key.GetLocation())] = key
        it.GoToNextItem()

    assert ("reg_str", "cvista.test") in recovered
    assert ("reg_int", "cvista.test") in recovered
    assert ("reg_dbl_vec", "cvista.test") in recovered
    assert info.Get(recovered[("reg_str", "cvista.test")]) == "value"
    assert info.Get(recovered[("reg_int", "cvista.test")]) == 42
    assert info.Get(recovered[("reg_dbl_vec", "cvista.test")]) == (1.0, 2.0, 3.0)


def test_makekey_subprocess_shutdown_no_sigsegv():
    """Creating keys via MakeKey must not segfault at interpreter exit (#249)."""
    code = """
import gc
import cvista
from cvista.util.keys import (
    DoubleKey,
    DoubleVectorKey,
    ExecutivePortKey,
    IdTypeKey,
    InformationKey,
    IntegerKey,
    MakeKey,
    ObjectBaseKey,
    RequestKey,
    StringKey,
)

info = cvista.vtkInformation()
s_key = MakeKey(StringKey, "demo_s", "cvista.subprocess")
i_key = MakeKey(IntegerKey, "demo_i", "cvista.subprocess")
d_key = MakeKey(DoubleKey, "demo_d", "cvista.subprocess")
dv_key = MakeKey(DoubleVectorKey, "demo_dv", "cvista.subprocess", 2)
id_key = MakeKey(IdTypeKey, "demo_id", "cvista.subprocess")
info_key = MakeKey(InformationKey, "demo_info", "cvista.subprocess")
obj_key = MakeKey(ObjectBaseKey, "demo_obj", "cvista.subprocess", "vtkObject")
req_key = MakeKey(RequestKey, "demo_req", "cvista.subprocess")
port_key = MakeKey(ExecutivePortKey, "demo_port", "cvista.subprocess")

info.Set(s_key, "ok")
info.Set(i_key, 7)
del s_key
gc.collect()
"""
    proc = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert proc.returncode == 0, (
        f"Subprocess failed with returncode={proc.returncode}\n"
        f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
    )


def test_repair_windows_passes_no_mangle_all(tmp_path):
    """Windows delvewheel repair must preserve canonical DLL names for cvista-sdk (#299)."""
    repo_root = Path(__file__).resolve().parents[2]
    repair_script = repo_root / "ci" / "cibw" / "repair_windows.py"
    if not repair_script.is_file():
        pytest.skip("ci/cibw/repair_windows.py not present in checkout")

    import importlib.util

    spec = importlib.util.spec_from_file_location("repair_windows", repair_script)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    fake_wheel = tmp_path / "cvista-0.2.0-cp313-cp313-win_amd64.whl"
    fake_wheel.write_bytes(b"")
    dest_dir = tmp_path / "wheelhouse"
    dest_dir.mkdir()

    recorded_cmds: list[list[str]] = []

    def fake_run(cmd, check=False):
        recorded_cmds.append(list(cmd))

    with (
        patch.object(module, "_bin_dirs", return_value=[str(tmp_path / "bin")]),
        patch.object(module, "audit_python_imports"),
        patch("subprocess.run", side_effect=fake_run),
    ):
        rc = module.main([str(tmp_path), str(dest_dir), str(fake_wheel)])

    assert rc == 0
    assert len(recorded_cmds) == 1
    cmd = recorded_cmds[0]
    assert cmd[:3] == ["delvewheel", "repair", "--no-mangle-all"]


def test_concurrent_lazy_build_links_and_cells():
    """Concurrent first-access BuildLinks/BuildCells must not race or rebuild in place (#223)."""
    sphere = cvista.vtkSphereSource()
    sphere.SetThetaResolution(24)
    sphere.SetPhiResolution(24)
    sphere.Update()

    for editable in (False, True):
        poly = cvista.vtkPolyData()
        poly.DeepCopy(sphere.GetOutput())
        poly.SetEditable(editable)
        n_pts = poly.GetNumberOfPoints()
        n_cells = poly.GetNumberOfCells()

        barrier = threading.Barrier(8)

        def worker(_tid: int) -> tuple[int, int]:
            barrier.wait()
            cell_ids = cvista.vtkIdList()
            pt_ids = cvista.vtkIdList()
            total_neighbors = 0
            total_pts = 0
            for pid in range(n_pts):
                poly.GetPointCells(pid, cell_ids)
                total_neighbors += cell_ids.GetNumberOfIds()
            for cid in range(n_cells):
                poly.GetCellPoints(cid, pt_ids)
                total_pts += pt_ids.GetNumberOfIds()
            return total_neighbors, total_pts

        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(worker, range(8)))

        assert len(set(results)) == 1
        assert results[0][0] > 0
        assert results[0][1] > 0

    # Verify explicit BuildLinks() still rebuilds when geometry is modified
    poly = cvista.vtkPolyData()
    poly.DeepCopy(sphere.GetOutput())
    poly.BuildLinks()
    first_build_time = poly.GetLinks().GetBuildTime()
    poly.Modified()
    poly.BuildLinks()
    assert poly.GetLinks().GetBuildTime() > first_build_time


def test_concurrent_unstructured_grid_build_links():
    """Concurrent first-access GetPointCells on vtkUnstructuredGrid is thread-safe (#223)."""
    sphere = cvista.vtkSphereSource()
    sphere.SetThetaResolution(16)
    sphere.SetPhiResolution(16)
    append = cvista.vtkAppendFilter()
    append.AddInputConnection(sphere.GetOutputPort())
    append.Update()

    ug = cvista.vtkUnstructuredGrid()
    ug.DeepCopy(append.GetOutput())
    n_pts = ug.GetNumberOfPoints()

    barrier = threading.Barrier(8)

    def worker(_tid: int) -> int:
        barrier.wait()
        cell_ids = cvista.vtkIdList()
        total = 0
        for pid in range(n_pts):
            ug.GetPointCells(pid, cell_ids)
            total += cell_ids.GetNumberOfIds()
        return total

    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(worker, range(8)))

    assert len(set(results)) == 1
    assert results[0] > 0


def test_connectivity_region_id_zero_initialized():
    """Unvisited points/cells in connectivity filters must be deterministically 0 (#221/#229)."""
    s1 = cvista.vtkSphereSource()
    s1.SetCenter(0.0, 0.0, 0.0)
    s2 = cvista.vtkSphereSource()
    s2.SetCenter(5.0, 0.0, 0.0)

    append_pd = cvista.vtkAppendPolyData()
    append_pd.AddInputConnection(s1.GetOutputPort())
    append_pd.AddInputConnection(s2.GetOutputPort())
    append_pd.Update()

    # vtkPolyDataConnectivityFilter with PointSeededRegions only visits one component
    pd_conn = cvista.vtkPolyDataConnectivityFilter()
    pd_conn.SetInputData(append_pd.GetOutput())
    pd_conn.ColorRegionsOn()
    pd_conn.SetExtractionModeToPointSeededRegions()
    pd_conn.AddSeed(0)
    pd_conn.Update()
    pd_out = pd_conn.GetOutput()
    pd_region = pd_out.GetPointData().GetArray("RegionId")
    assert pd_region is not None
    vals = [pd_region.GetTuple1(i) for i in range(pd_region.GetNumberOfTuples())]
    assert set(vals) <= {0.0}

    # vtkConnectivityFilter with PointSeededRegions
    conn = cvista.vtkConnectivityFilter()
    conn.SetInputData(append_pd.GetOutput())
    conn.ColorRegionsOn()
    conn.SetExtractionModeToPointSeededRegions()
    conn.AddSeed(0)
    conn.Update()
    ug_out = conn.GetOutput()
    pt_region = ug_out.GetPointData().GetArray("RegionId")
    cell_region = ug_out.GetCellData().GetArray("RegionId")
    assert pt_region is not None
    assert cell_region is not None
    pt_vals = [pt_region.GetTuple1(i) for i in range(pt_region.GetNumberOfTuples())]
    cell_vals = [cell_region.GetTuple1(i) for i in range(cell_region.GetNumberOfTuples())]
    assert set(pt_vals) <= {0.0}
    assert set(cell_vals) <= {0.0}


def test_delaunay3d_enclosing_faces_on_32bit_point_cloud():
    """vtkDelaunay3D triangulation with degenerate point insertions (FindEnclosingFaces) (#221)."""
    rng = np.random.default_rng(12345)
    pts_np = rng.uniform(-1.0, 1.0, size=(80, 3)).astype(np.float32)

    pts = cvista.vtkPoints()
    for p in pts_np:
        pts.InsertNextPoint(float(p[0]), float(p[1]), float(p[2]))

    pd = cvista.vtkPolyData()
    pd.SetPoints(pts)

    delaunay = cvista.vtkDelaunay3D()
    delaunay.SetInputData(pd)
    delaunay.SetTolerance(1e-3)
    delaunay.Update()

    out = delaunay.GetOutput()
    assert out.GetNumberOfCells() > 0
