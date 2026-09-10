# Hidden triangulation edges under MSAA

[cvista #297](https://github.com/pyvista/cvista/issues/297) tracks thin diagonal
lines across polygon faces when surface edges and multisample antialiasing
(MSAA) are enabled. Related upstream reports are
[VTK #18064](https://gitlab.kitware.com/vtk/vtk/-/issues/18064) and
[VTK #18865](https://gitlab.kitware.com/vtk/vtk/-/issues/18865).

## Cause and correction

The OpenGL polydata mapper triangulates polygons for rendering and supplies a
mask distinguishing polygon boundaries from hidden triangle edges. Previously,
the fragment shader hid an edge by adding the line width to its signed distance.
With MSAA, a triangle can cover a sample without covering that pixel's center.
The center's negative distance can bring a masked edge back within the blend
radius, especially at small line widths. This produces the diagonal even with
lighting disabled.

Masked distances now use `lineWidth + 1`, which is strictly outside the
`0.5 + 0.5 * lineWidth` blend radius. Visible edges retain their original signed
distances, widths, and blending. This also honors explicit `vtkEdgeFlags`.

The edge shader no longer discards samples based on the pixel center. Its
geometry shader emits the original triangle without expansion, so rasterization
already determines coverage. A discard could remove valid MSAA samples and
interfere with derivatives used by subsequent shading. This removes the need for
the previous Apple-specific exception to that discard.

For edges rendered as tubes, clamp the square-root argument before evaluating
it. Away from a visible edge, the radius calculation may otherwise take the
square root of a negative value; clamping the result afterwards does not
portably remove the resulting NaN.

This is an intentional rendering correctness change, enabled by default, as
requested in #297. It is not a filter optimization or an `EnableFast()` path.
Pixels affected by the defect are expected to differ from stock VTK; geometry,
arrays, and numerical filter output are unchanged. Existing parity checks remain
in place without relaxed thresholds or exclusions.

## Validation

`tests/regression/test_hidden_triangulation_edges.py` constructs synthetic quads,
pentagons, and quads with all edges explicitly masked. It compares each polygon
interior with the same actor rendered without edges and separately requires
visible polygon boundaries to remain visible. The 72 cases cover MSAA on/off,
line widths 1/2/4, unlit/lit/tube rendering, and translucency. The tests allow one
8-bit framebuffer quantization step for lighting interpolation and fail on an
empty render or shader error.

On Linux, the unmodified cvista 9.7.0.4 wheel fails the same nine narrow-edge MSAA
cases under NVIDIA GTX 1080 (driver 580.173.02) and Mesa 25.2.6 llvmpipe. The fixed
manylinux wheel passes all 72 on both drivers; its complete regression suite
passes 217 tests on llvmpipe. The existing 13 rendering parity scenes remain
pixel-exact against stock VTK 9.7.0 on the same Mesa driver.
The numerical/parity suite passes 891 tests, and the hardware-selection parity
scene preserves all 54 selected IDs. At the pinned PyVista revision
`9e85d7045eb7455ac39437cdfea2ef08893b85b8`, the property/renderer suites and
selected plotting tests pass 174 tests with the same one existing skip on both
stock VTK and the fixed wheel.

Run the focused test against a built cvista wheel:

```sh
VTK_DEFAULT_OPENGL_WINDOW=vtkEGLRenderWindow \
  python -m pytest tests/regression/test_hidden_triangulation_edges.py
```

The standard `regression` CI job discovers the new file automatically.
