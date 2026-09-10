"""Hidden polygon diagonals must never contribute to surface-with-edges shading.

VTK #18064 / #18865: adding the line width to a hidden edge's signed distance
does not exclude it. With MSAA a covered sample can belong to a pixel whose
center is outside the triangle, so the negative distance exposes the diagonal.
Compare the polygon interior with the same actor's edges turned off; also check
that real boundary edges still render. No reference images or external meshes
are required.
"""

import numpy as np
import pytest

from cvista import (
    vtkActor,
    vtkCellArray,
    vtkDataSetAttributes,
    vtkOutputWindow,
    vtkPoints,
    vtkPolyData,
    vtkPolyDataMapper,
    vtkRenderer,
    vtkRenderWindow,
    vtkStringOutputWindow,
    vtkUnsignedCharArray,
    vtk_to_numpy,
)


SIZE = 256


@pytest.mark.parametrize("shape", ["quad", "pentagon", "masked_quad"])
@pytest.mark.parametrize("width", [1, 2, 4])
@pytest.mark.parametrize("samples", [0, 4])
@pytest.mark.parametrize("style", ["unlit", "lit", "tubes", "translucent"])
def test_hidden_triangulation_edges(shape, width, samples, style):
    if shape in ("quad", "masked_quad"):
        coordinates = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    else:
        count = 5
        angles = np.linspace(0, 2 * np.pi, count, endpoint=False)
        coordinates = [(1.4 * np.cos(a), 1.4 * np.sin(a), 0) for a in angles]

    points = vtkPoints()
    for point in coordinates:
        points.InsertNextPoint(point)
    polygons = vtkCellArray()
    polygons.InsertNextCell(len(coordinates), list(range(len(coordinates))))
    mesh = vtkPolyData()
    mesh.SetPoints(points)
    mesh.SetPolys(polygons)
    if shape == "masked_quad":
        flags = vtkUnsignedCharArray()
        flags.SetName("vtkEdgeFlags")
        flags.SetNumberOfValues(len(coordinates))
        flags.FillValue(0)
        mesh.GetPointData().SetAttribute(flags, vtkDataSetAttributes.EDGEFLAG)

    mapper = vtkPolyDataMapper()
    mapper.SetInputData(mesh)
    actor = vtkActor()
    actor.SetMapper(mapper)
    prop = actor.GetProperty()
    prop.SetColor(0.7, 0.6, 0.4)
    prop.SetEdgeColor(0, 0, 0)
    prop.SetLineWidth(width)
    prop.SetLighting(style in ("lit", "tubes"))
    prop.SetRenderLinesAsTubes(style == "tubes")
    prop.SetOpacity(0.5 if style == "translucent" else 1.0)

    renderer = vtkRenderer()
    renderer.SetBackground(0.9, 0.9, 0.9)
    renderer.AddActor(actor)
    window = vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(SIZE, SIZE)
    window.SetMultiSamples(samples)
    window.AddRenderer(renderer)
    camera = renderer.GetActiveCamera()
    camera.SetPosition(0, 0, 5)
    camera.SetFocalPoint(0, 0, 0)
    camera.SetViewUp(0, 1, 0)
    camera.Azimuth(15)
    camera.Elevation(20)
    camera.Roll(17)
    renderer.ResetCameraClippingRange()

    def capture():
        window.Render()
        pixels = vtkUnsignedCharArray()
        window.GetRGBACharPixelData(0, 0, SIZE - 1, SIZE - 1, 0, pixels)
        return vtk_to_numpy(pixels).reshape(SIZE, SIZE, 4).astype(np.int16)

    previous_output = vtkOutputWindow.GetInstance()
    diagnostics = vtkStringOutputWindow()
    vtkOutputWindow.SetInstance(diagnostics)
    try:
        prop.EdgeVisibilityOff()
        surface = capture()
        prop.EdgeVisibilityOn()
        edged = capture()

        # Project the original polygon boundary, not its rendering triangles.
        boundary = []
        for point in coordinates:
            renderer.SetWorldPoint(*point, 1)
            renderer.WorldToDisplay()
            boundary.append(renderer.GetDisplayPoint()[:2])
        boundary = np.asarray(boundary)
        y, x = np.mgrid[:SIZE, :SIZE] + 0.5
        distances = []
        for start, end in zip(boundary, np.roll(boundary, -1, axis=0)):
            dx, dy = end - start
            distances.append((dx * (y - start[1]) - dy * (x - start[0])) / np.hypot(dx, dy))
        distance = np.min(distances, axis=0)
        interior = distance > width + 4
        assert np.count_nonzero(interior) > 1000, "test polygon did not reach the framebuffer"
        assert np.any(surface[interior, :3] < 200), "only background was rendered"

        delta = np.max(np.abs(edged - surface), axis=2)
        # Allow one framebuffer quantization step for lighting interpolation.
        assert delta[interior].max() <= 1, "hidden edges changed the polygon interior"
        if shape == "masked_quad":
            assert delta.max() <= 1, "explicitly masked boundary edges are still visible"
        else:
            boundary_band = (distance >= 0) & ~interior
            assert np.count_nonzero(delta[boundary_band] > 8) > 100, "visible edges disappeared"
        assert "ERR|" not in diagnostics.GetOutput(), diagnostics.GetOutput()
    finally:
        window.Finalize()
        vtkOutputWindow.SetInstance(previous_output)
