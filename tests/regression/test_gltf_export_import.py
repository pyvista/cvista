"""Regression: glTF export and import/read round-trips geometry and attributes (#119).

``vtkGLTFExporter`` writes triangle/line/vertex connectivity indices into a
``vtkUnsignedIntArray`` and serializes them via ``vtkGLTFWriterUtils::WriteValues``
(either to an external ``.bin`` stream when ``InlineData=False`` or to a
``vtkBase64OutputStream`` when ``InlineData=True``).

When ``CVISTA_DISPATCH_MINIMAL`` trimmed ``vtkUnsignedIntArray`` out of
``vtkArrayDispatch::AOSArrays``, ``WriteValues`` needed an explicit
``ToAOSDataArray()->GetVoidPointer(0)`` fallback so ``uint32`` index buffers are
written losslessly instead of being dropped with a warning (#119, #127).

This test verifies both external-buffer and inline-base64 glTF exports across a
multi-actor scene with ``int32`` and ``int64`` input cell connectivity, point
normals, batch IDs, texture coordinates, and mapped vertex colors, checking
round-trip fidelity through both ``vtkGLTFReader`` and ``vtkGLTFImporter``.
"""

from __future__ import annotations

import numpy as np
import pytest

from cvista.util.numpy_support import numpy_to_vtk, vtk_to_numpy
from cvista.vtkCommonCore import (
    vtkFloatArray,
    vtkOutputWindow,
    vtkPoints,
    vtkStringOutputWindow,
)
from cvista.vtkCommonDataModel import vtkCellArray, vtkPolyData
from cvista.vtkFiltersCore import vtkPolyDataNormals
from cvista.vtkFiltersSources import vtkConeSource, vtkSphereSource
from cvista.vtkIOExport import vtkGLTFExporter
from cvista.vtkIOImport import vtkGLTFImporter, vtkGLTFReader
from cvista.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderer,
    vtkRenderWindow,
)


def _make_sphere_polydata(*, use_64bit_ids: bool) -> vtkPolyData:
    src = vtkSphereSource()
    src.SetThetaResolution(16)
    src.SetPhiResolution(16)
    src.Update()

    normals_f = vtkPolyDataNormals()
    normals_f.SetInputData(src.GetOutput())
    normals_f.ComputePointNormalsOn()
    normals_f.SplittingOff()
    normals_f.Update()
    pd = normals_f.GetOutput()

    polys = vtkCellArray()
    if use_64bit_ids:
        polys.Use64BitStorage()
    else:
        polys.Use32BitStorage()
    polys.DeepCopy(pd.GetPolys())
    pd.SetPolys(polys)

    n_pts = pd.GetNumberOfPoints()

    # Rename normals array to "NORMAL" so vtkGLTFExporter(SaveNormal=True) emits it.
    norm_arr = pd.GetPointData().GetNormals()
    norm_arr.SetName("NORMAL")

    batch = numpy_to_vtk(np.arange(n_pts, dtype=np.float32), deep=True)
    batch.SetName("_BATCHID")
    pd.GetPointData().AddArray(batch)

    pts_np = vtk_to_numpy(pd.GetPoints().GetData())
    tcoords_np = np.column_stack(
        [(pts_np[:, 0] + 0.5), (pts_np[:, 1] + 0.5)]
    ).astype(np.float32)
    tcoords = numpy_to_vtk(tcoords_np, deep=True)
    tcoords.SetName("TEXCOORD_0")
    pd.GetPointData().SetTCoords(tcoords)

    scalars = numpy_to_vtk(np.linspace(0.0, 1.0, n_pts, dtype=np.float32), deep=True)
    scalars.SetName("elev")
    pd.GetPointData().SetScalars(scalars)
    return pd


def _make_cone_polydata(*, use_64bit_ids: bool) -> vtkPolyData:
    cone = vtkConeSource()
    cone.SetResolution(12)
    cone.CappingOn()
    cone.Update()

    # Build an explicit triangle mesh with known connectivity storage width.
    pts = vtkPoints()
    pts.DeepCopy(cone.GetOutput().GetPoints())
    src_polys = cone.GetOutput().GetPolys()

    tris = vtkCellArray()
    if use_64bit_ids:
        tris.Use64BitStorage()
    else:
        tris.Use32BitStorage()

    conn = vtk_to_numpy(src_polys.GetConnectivityArray())
    offs = vtk_to_numpy(src_polys.GetOffsetsArray())
    for c in range(len(offs) - 1):
        cell_pts = conn[offs[c] : offs[c + 1]]
        # Fan-triangulate any polygon with >3 vertices
        for k in range(1, len(cell_pts) - 1):
            tris.InsertNextCell(3, [int(cell_pts[0]), int(cell_pts[k]), int(cell_pts[k + 1])])

    pd = vtkPolyData()
    pd.SetPoints(pts)
    pd.SetPolys(tris)

    n_pts = pd.GetNumberOfPoints()
    normals = vtkFloatArray()
    normals.SetName("NORMAL")
    normals.SetNumberOfComponents(3)
    normals.SetNumberOfTuples(n_pts)
    normals_np = np.tile(np.array([[0.0, 0.0, 1.0]], dtype=np.float32), (n_pts, 1))
    for i in range(n_pts):
        normals.SetTuple3(i, float(normals_np[i, 0]), float(normals_np[i, 1]), float(normals_np[i, 2]))
    pd.GetPointData().SetNormals(normals)

    batch = numpy_to_vtk(np.arange(100, 100 + n_pts, dtype=np.float32), deep=True)
    batch.SetName("_BATCHID")
    pd.GetPointData().AddArray(batch)
    return pd


def _iter_polydata_leaves(multiblock) -> list[vtkPolyData]:
    leaves: list[vtkPolyData] = []
    it = multiblock.NewIterator()
    it.InitTraversal()
    while not it.IsDoneWithTraversal():
        obj = it.GetCurrentDataObject()
        if isinstance(obj, vtkPolyData):
            leaves.append(obj)
        it.GoToNextItem()
    return leaves


@pytest.mark.parametrize("inline_data", [False, True])
@pytest.mark.parametrize("use_64bit_ids", [False, True])
def test_gltf_export_import_roundtrip(tmp_path, inline_data: bool, use_64bit_ids: bool):
    sphere_pd = _make_sphere_polydata(use_64bit_ids=use_64bit_ids)
    cone_pd = _make_cone_polydata(use_64bit_ids=use_64bit_ids)

    ren = vtkRenderer()
    rw = vtkRenderWindow()
    rw.SetOffScreenRendering(1)
    rw.AddRenderer(ren)

    for pd in (sphere_pd, cone_pd):
        mapper = vtkPolyDataMapper()
        mapper.SetInputData(pd)
        actor = vtkActor()
        actor.SetMapper(mapper)
        ren.AddActor(actor)

    gltf_path = tmp_path / f"scene_inline{int(inline_data)}_64bit{int(use_64bit_ids)}.gltf"

    prev_out = vtkOutputWindow.GetInstance()
    diag = vtkStringOutputWindow()
    vtkOutputWindow.SetInstance(diag)
    try:
        exporter = vtkGLTFExporter()
        exporter.SetRenderWindow(rw)
        exporter.SetFileName(str(gltf_path))
        exporter.SetInlineData(inline_data)
        exporter.SetSaveNormal(True)
        exporter.SetSaveBatchId(True)
        exporter.Write()

        assert "Failed to write data array" not in diag.GetOutput()

        assert gltf_path.is_file()
        if not inline_data:
            bin_files = list(tmp_path.glob("*.bin"))
            assert len(bin_files) >= 1
            assert all(bf.stat().st_size > 0 for bf in bin_files)

        # 1. Verify via vtkGLTFReader (reads the scene hierarchy into a MultiBlockDataSet).
        reader = vtkGLTFReader()
        reader.SetFileName(str(gltf_path))
        reader.Update()
        leaves = _iter_polydata_leaves(reader.GetOutput())
        assert len(leaves) == 2

        for orig, read_pd in zip((sphere_pd, cone_pd), leaves):
            assert read_pd.GetNumberOfPoints() == orig.GetNumberOfPoints()
            assert read_pd.GetNumberOfPolys() == orig.GetNumberOfPolys()

            orig_pts = vtk_to_numpy(orig.GetPoints().GetData())
            read_pts = vtk_to_numpy(read_pd.GetPoints().GetData())
            np.testing.assert_allclose(read_pts, orig_pts, rtol=1e-6, atol=1e-6)

            orig_conn = vtk_to_numpy(orig.GetPolys().GetConnectivityArray())
            read_conn = vtk_to_numpy(read_pd.GetPolys().GetConnectivityArray())
            np.testing.assert_array_equal(read_conn, orig_conn)

            orig_offs = vtk_to_numpy(orig.GetPolys().GetOffsetsArray())
            read_offs = vtk_to_numpy(read_pd.GetPolys().GetOffsetsArray())
            np.testing.assert_array_equal(read_offs, orig_offs)

            read_normals = read_pd.GetPointData().GetNormals()
            assert read_normals is not None
            np.testing.assert_allclose(
                vtk_to_numpy(read_normals),
                vtk_to_numpy(orig.GetPointData().GetArray("NORMAL")),
                rtol=1e-6,
                atol=1e-6,
            )

            read_batch = read_pd.GetPointData().GetArray("_BATCHID")
            assert read_batch is not None
            np.testing.assert_allclose(
                vtk_to_numpy(read_batch),
                vtk_to_numpy(orig.GetPointData().GetArray("_BATCHID")),
                rtol=1e-6,
                atol=1e-6,
            )

        # Sphere also had texture coordinates and scalar-mapped vertex colors.
        sphere_read = leaves[0]
        assert sphere_read.GetPointData().GetTCoords() is not None
        np.testing.assert_allclose(
            vtk_to_numpy(sphere_read.GetPointData().GetTCoords()),
            vtk_to_numpy(sphere_pd.GetPointData().GetTCoords()),
            rtol=1e-6,
            atol=1e-6,
        )
        assert sphere_read.GetPointData().GetArray("COLOR_0") is not None

        # 2. Verify via vtkGLTFImporter (reconstructs actors/mappers in a target RenderWindow).
        imp_rw = vtkRenderWindow()
        imp_rw.SetOffScreenRendering(1)
        importer = vtkGLTFImporter()
        importer.SetFileName(str(gltf_path))
        importer.SetRenderWindow(imp_rw)
        importer.Update()

        imp_ren = imp_rw.GetRenderers().GetFirstRenderer()
        assert imp_ren is not None
        actors = imp_ren.GetActors()
        assert actors.GetNumberOfItems() == 2

        by_npts = {orig.GetNumberOfPoints(): orig for orig in (sphere_pd, cone_pd)}
        seen_npts = set()
        actors.InitTraversal()
        for _ in range(2):
            act = actors.GetNextActor()
            assert act is not None
            imp_pd = act.GetMapper().GetInput()
            npts = imp_pd.GetNumberOfPoints()
            assert npts in by_npts
            seen_npts.add(npts)
            orig = by_npts[npts]
            assert imp_pd.GetNumberOfPolys() == orig.GetNumberOfPolys()
            np.testing.assert_allclose(
                vtk_to_numpy(imp_pd.GetPoints().GetData()),
                vtk_to_numpy(orig.GetPoints().GetData()),
                rtol=1e-6,
                atol=1e-6,
            )
            np.testing.assert_array_equal(
                vtk_to_numpy(imp_pd.GetPolys().GetConnectivityArray()),
                vtk_to_numpy(orig.GetPolys().GetConnectivityArray()),
            )
        assert seen_npts == set(by_npts.keys())
    finally:
        vtkOutputWindow.SetInstance(prev_out)

