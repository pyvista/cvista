"""Pixel-exact RENDER scene registry for the fvtk vs stock-VTK regression suite.

Sister to ``tests/bitexact`` but for the rendering pipeline. Every scene here is
built against the *vtkmodules* API only (no pyvista, no ``import vtk``), so the
exact same source drives two backends:

  * stock VTK 9.6.2   -- ``vtkmodules`` resolves to the upstream wheel
  * fvtk (this fork)  -- ``tools/fvtk_shim.py`` redirects ``vtkmodules.* -> fvtk.*``

The only thing that differs between the two render runs is the compiled C++
backend. CRITICAL CONTROL: both sides must render against the *same* EGL/GL
software driver (the host nix Mesa / llvmpipe), so a pixel diff reflects only
fvtk's code, not a different bundled Mesa. The driver records GL_RENDERER /
GL_VERSION on each side; the comparison asserts they match before trusting any
pixel diff.

Determinism rules (so the GL command stream is bit-identical on both sides):
  * Fixed window size, ``SetMultiSamples(0)`` (no MSAA sample-order ambiguity).
  * Fixed deterministic background, fixed camera (explicit position/focal/up,
    no ResetCamera heuristics that could differ).
  * Geometry from deterministic sources (sphere/cone/plane) with fixed
    resolutions; scalars from pure integer/linspace algebra (no sin/cos).
  * No time-, pointer-, or hash-seeded inputs.

Each scene is ``fn() -> (renderer, render_window)`` already wired and sized. The
driver renders it offscreen, reads back the RGBA framebuffer and the Z buffer
via vtkWindowToImageFilter, and dumps both. ``compare.py`` then asserts exact
byte equality of the RGBA buffer (and the Z buffer where captured).
"""
from __future__ import annotations

import numpy as np

# --- vtkmodules imports (resolve to stock vtk OR fvtk depending on the venv) ---
from vtkmodules.vtkCommonCore import vtkFloatArray, vtkPoints, vtkLookupTable
from vtkmodules.vtkCommonDataModel import vtkPolyData, vtkCellArray
from vtkmodules.vtkFiltersSources import (
    vtkConeSource,
    vtkSphereSource,
    vtkPlaneSource,
)
from vtkmodules.vtkFiltersCore import vtkGlyph3D, vtkTubeFilter
from vtkmodules.vtkRenderingCore import (
    vtkRenderer,
    vtkRenderWindow,
    vtkPolyDataMapper,
    vtkActor,
)

# Register the OpenGL factory overrides (vtkRenderWindow -> vtkEGLRenderWindow,
# mappers -> vtkOpenGLPolyDataMapper, etc.). Without this the abstract
# RenderingCore classes have no concrete GL backend.
import vtkmodules.vtkRenderingOpenGL2  # noqa: F401

WIN_W = 256
WIN_H = 256


def _new_window(ren):
    rw = vtkRenderWindow()
    rw.SetOffScreenRendering(1)
    rw.SetMultiSamples(0)
    rw.SetSize(WIN_W, WIN_H)
    rw.AddRenderer(ren)
    return rw


def _fixed_camera(ren, dist=4.0):
    """Deterministic camera -- explicit params, never ResetCamera heuristics."""
    cam = ren.GetActiveCamera()
    cam.SetPosition(dist, 0.6 * dist, 0.8 * dist)
    cam.SetFocalPoint(0.0, 0.0, 0.0)
    cam.SetViewUp(0.0, 1.0, 0.0)
    cam.SetViewAngle(30.0)
    cam.SetClippingRange(0.1, 100.0)


def _renderer(bg=(0.12, 0.18, 0.27)):
    ren = vtkRenderer()
    ren.SetBackground(*bg)
    return ren


# --------------------------------------------------------------------------
# Scenes
# --------------------------------------------------------------------------
def scene_sphere_shaded():
    """A shaded polydata surface (Gouraud-lit sphere)."""
    s = vtkSphereSource()
    s.SetThetaResolution(48)
    s.SetPhiResolution(48)
    s.SetRadius(1.0)
    m = vtkPolyDataMapper()
    m.SetInputConnection(s.GetOutputPort())
    a = vtkActor()
    a.SetMapper(m)
    a.GetProperty().SetColor(0.85, 0.55, 0.30)
    a.GetProperty().SetAmbient(0.18)
    a.GetProperty().SetDiffuse(0.75)
    a.GetProperty().SetSpecular(0.35)
    a.GetProperty().SetSpecularPower(25.0)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=3.2)
    return ren, _new_window(ren)


def scene_cone_shaded():
    """A shaded cone (different primitive count / normals path)."""
    c = vtkConeSource()
    c.SetResolution(40)
    c.SetHeight(1.5)
    c.SetRadius(0.7)
    m = vtkPolyDataMapper()
    m.SetInputConnection(c.GetOutputPort())
    a = vtkActor()
    a.SetMapper(m)
    a.GetProperty().SetColor(0.30, 0.70, 0.85)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=3.0)
    return ren, _new_window(ren)


def scene_scalars_lut():
    """Surface colored by scalars through a lookup table."""
    s = vtkSphereSource()
    s.SetThetaResolution(40)
    s.SetPhiResolution(40)
    s.Update()
    pd = s.GetOutput()
    n = pd.GetNumberOfPoints()
    # Deterministic scalar field: z-coordinate index algebra (no trig).
    sc = vtkFloatArray()
    sc.SetName("scal")
    sc.SetNumberOfTuples(n)
    pts = pd.GetPoints()
    for i in range(n):
        z = pts.GetPoint(i)[2]
        sc.SetValue(i, float(z))
    pd.GetPointData().SetScalars(sc)
    lut = vtkLookupTable()
    lut.SetNumberOfColors(256)
    lut.SetHueRange(0.667, 0.0)
    lut.SetTableRange(-1.0, 1.0)
    lut.Build()
    m = vtkPolyDataMapper()
    m.SetInputData(pd)
    m.SetLookupTable(lut)
    m.SetScalarRange(-1.0, 1.0)
    m.SetScalarModeToUsePointData()
    m.SetColorModeToMapScalars()
    a = vtkActor()
    a.SetMapper(m)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=3.2)
    return ren, _new_window(ren)


def scene_points_glyph():
    """A points/glyph actor: small spheres glyphed onto a point cloud."""
    # Deterministic point cloud on a coarse grid.
    pts = vtkPoints()
    g = 6
    for ix in range(g):
        for iy in range(g):
            for iz in range(g):
                pts.InsertNextPoint(
                    (ix - (g - 1) / 2.0) * 0.4,
                    (iy - (g - 1) / 2.0) * 0.4,
                    (iz - (g - 1) / 2.0) * 0.4,
                )
    cloud = vtkPolyData()
    cloud.SetPoints(pts)
    src = vtkSphereSource()
    src.SetThetaResolution(8)
    src.SetPhiResolution(8)
    src.SetRadius(0.08)
    glyph = vtkGlyph3D()
    glyph.SetInputData(cloud)
    glyph.SetSourceConnection(src.GetOutputPort())
    glyph.SetScaleModeToDataScalingOff()
    m = vtkPolyDataMapper()
    m.SetInputConnection(glyph.GetOutputPort())
    a = vtkActor()
    a.SetMapper(m)
    a.GetProperty().SetColor(0.9, 0.8, 0.2)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=4.0)
    return ren, _new_window(ren)


def scene_tube_lines():
    """A thin-line / tube actor along a deterministic polyline."""
    pts = vtkPoints()
    npts = 60
    for i in range(npts):
        t = i / (npts - 1.0)
        # Deterministic helix-free zigzag (no trig): piecewise-linear curve.
        x = 2.0 * t - 1.0
        y = 0.5 * (((i % 8) / 7.0) - 0.5)
        z = 0.3 * (((i % 5) / 4.0) - 0.5)
        pts.InsertNextPoint(x, y, z)
    lines = vtkCellArray()
    lines.InsertNextCell(npts)
    for i in range(npts):
        lines.InsertCellPoint(i)
    poly = vtkPolyData()
    poly.SetPoints(pts)
    poly.SetLines(lines)
    tube = vtkTubeFilter()
    tube.SetInputData(poly)
    tube.SetRadius(0.04)
    tube.SetNumberOfSides(12)
    m = vtkPolyDataMapper()
    m.SetInputConnection(tube.GetOutputPort())
    a = vtkActor()
    a.SetMapper(m)
    a.GetProperty().SetColor(0.2, 0.9, 0.5)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=2.6)
    return ren, _new_window(ren)


def scene_edges():
    """A simple actor with edges drawn (wireframe-on-surface)."""
    s = vtkSphereSource()
    s.SetThetaResolution(16)
    s.SetPhiResolution(16)
    m = vtkPolyDataMapper()
    m.SetInputConnection(s.GetOutputPort())
    a = vtkActor()
    a.SetMapper(m)
    a.GetProperty().SetColor(0.7, 0.3, 0.3)
    a.GetProperty().EdgeVisibilityOn()
    a.GetProperty().SetEdgeColor(0.95, 0.95, 0.95)
    a.GetProperty().SetLineWidth(1.0)
    ren = _renderer()
    ren.AddActor(a)
    _fixed_camera(ren, dist=3.2)
    return ren, _new_window(ren)


SCENES = {
    "sphere_shaded": scene_sphere_shaded,
    "cone_shaded": scene_cone_shaded,
    "scalars_lut": scene_scalars_lut,
    "points_glyph": scene_points_glyph,
    "tube_lines": scene_tube_lines,
    "edges": scene_edges,
}


def iter_scenes():
    for name in SCENES:
        yield name
