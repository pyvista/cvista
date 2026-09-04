"""Regression: vtkSSAOPass must not depend on a volume's Shade flag.

vtkSSAOPass::PostReplaceShaderValues unconditionally substituted a reference to
``shading_gradient`` into the volume mapper's fragment shader:

    g_dataNormal = -shading_gradient.xyz;

vtkVolumeShaderComposer only declares ``shading_gradient`` when the volume
property has Shade enabled and the mapper uses a composite-family blend mode.
For any volume rendered without shading, the substituted line referenced an
undeclared variable and the fragment shader failed to compile and link. The
volume silently disappeared from the render (no exception is raised; the
failure surfaces as a driver-logged shader error and an all-background frame).

The fix reads whether the mapper's shader actually declares
``vec4 shading_gradient`` before substituting the reference, and substitutes a
constant camera-facing normal when it does not.

The observable failure is the shader build, not the frame: vtkSSAOPass on a
volume renders an all-background frame under software EGL whether or not the
shader links, because its multi-pass compositing contributes no visible pixels
through llvmpipe (stock VTK 9.6.2 behaves identically here). So this test does
not assert on framebuffer contents. It captures VTK's error log across the
render and asserts the volume fragment shader compiles and links: a regression
dumps the failing shader source and the driver's ``shading_gradient`` error
through vtkShaderProgram::ReportShaderError, so the marker's presence in the log
is the crash this fix targets. A module fixture first renders a plain volume
(no SSAO) and skips if even that is blank, so the assertions are never vacuous.
"""

import numpy as np
import pytest

from cvista.util.numpy_support import vtk_to_numpy
from cvista.vtkCommonCore import (
    vtkFloatArray,
    vtkOutputWindow,
    vtkStringOutputWindow,
    vtkUnsignedCharArray,
)
from cvista.vtkCommonDataModel import vtkImageData, vtkPiecewiseFunction
from cvista.vtkRenderingCore import (
    vtkColorTransferFunction,
    vtkRenderWindow,
    vtkVolume,
    vtkVolumeProperty,
)
import cvista.vtkRenderingOpenGL2  # noqa: F401  (registers the OpenGL factory)
from cvista.vtkRenderingOpenGL2 import vtkCameraPass, vtkOpenGLRenderer, vtkSSAOPass
from cvista.vtkRenderingVolumeOpenGL2 import vtkOpenGLGPUVolumeRayCastMapper

WINDOW_SIZE = 64
BACKGROUND = (0.05, 0.05, 0.05)

# A fragment-shader build failure routes through vtkShaderProgram: a compile
# error dumps the numbered source and the driver message (both name
# ``shading_gradient`` for the bug this guards) via ReportShaderError, and a link
# error is prefixed "Links failed". Either marker in the render's error log is a
# regression; neither appears once the shader compiles and links.
_SHADER_BUILD_FAILURE = ('shading_gradient', 'Links failed')


class _Diagnostics:
    """Capture every vtkErrorMacro raised inside the ``with`` block."""

    def __init__(self):
        self.text = ''

    def __enter__(self):
        self._recorder = vtkStringOutputWindow()
        self._previous = vtkOutputWindow.GetInstance()
        vtkOutputWindow.SetInstance(self._recorder)
        return self

    def __exit__(self, *_exc):
        self.text = self._recorder.GetOutput() or ''
        vtkOutputWindow.SetInstance(self._previous)
        return False


def _make_volume_data(size=16):
    """A small scalar field with real gradients (not a centered blob)."""
    image = vtkImageData()
    image.SetDimensions(size, size, size)
    image.SetSpacing(1.0, 1.0, 1.0)
    image.SetOrigin(0.0, 0.0, 0.0)

    scalars = vtkFloatArray()
    scalars.SetName("scalars")
    scalars.SetNumberOfTuples(size * size * size)
    idx = 0
    for k in range(size):
        for j in range(size):
            for i in range(size):
                # Off-center, anisotropic ramp so every axis has a distinct
                # gradient direction; avoids the classic centered-sphere trap.
                value = 0.6 * i + 0.3 * j + 0.1 * k
                scalars.SetValue(idx, value)
                idx += 1
    image.GetPointData().SetScalars(scalars)
    return image


def _make_volume(*, shade):
    image = _make_volume_data()

    color_tf = vtkColorTransferFunction()
    color_tf.AddRGBPoint(0.0, 0.1, 0.2, 0.8)
    color_tf.AddRGBPoint(15.0, 0.9, 0.6, 0.1)

    opacity_tf = vtkPiecewiseFunction()
    opacity_tf.AddPoint(0.0, 0.0)
    opacity_tf.AddPoint(15.0, 0.8)

    prop = vtkVolumeProperty()
    prop.SetColor(color_tf)
    prop.SetScalarOpacity(opacity_tf)
    prop.SetInterpolationTypeToLinear()
    if shade:
        prop.ShadeOn()
    else:
        prop.ShadeOff()

    mapper = vtkOpenGLGPUVolumeRayCastMapper()
    mapper.SetInputData(image)
    mapper.SetBlendModeToComposite()

    volume = vtkVolume()
    volume.SetMapper(mapper)
    volume.SetProperty(prop)
    return volume


def _off_axis_camera(renderer):
    camera = renderer.GetActiveCamera()
    camera.SetPosition(30, 22, 18)
    camera.SetFocalPoint(8, 8, 8)
    camera.SetViewUp(0, 0, 1)
    renderer.ResetCameraClippingRange()


def _render_ssao(*, shade):
    """Render one frame of a volume with SSAO on, capturing VTK's error log.

    The capture spans the frame that first builds the shader, so a shader that
    fails to compile or link is reported here. Skips if the host cannot create a
    usable OpenGL context.
    """
    renderer = vtkOpenGLRenderer()
    renderer.AddVolume(_make_volume(shade=shade))
    renderer.SetBackground(*BACKGROUND)
    _off_axis_camera(renderer)

    ssao_pass = vtkSSAOPass()
    ssao_pass.SetDelegatePass(vtkCameraPass())
    ssao_pass.SetRadius(4.0)
    ssao_pass.SetKernelSize(16)
    renderer.SetPass(ssao_pass)

    ren_win = vtkRenderWindow()
    ren_win.SetOffScreenRendering(1)
    ren_win.SetSize(WINDOW_SIZE, WINDOW_SIZE)
    ren_win.AddRenderer(renderer)

    with _Diagnostics() as diagnostics:
        try:
            ren_win.Render()
        except Exception as exc:  # pragma: no cover - host without a GL backend
            pytest.skip("could not create an OpenGL context: %s" % exc)
    if not ren_win.SupportsOpenGL():
        ren_win.Finalize()
        pytest.skip("OpenGL context is not usable on this host")
    ren_win.Finalize()
    return diagnostics.text


@pytest.fixture
def volume_renders():
    """Skip the module unless a plain volume (no SSAO) reaches the framebuffer.

    Every assertion below is "the shader built"; none reads the frame, because
    SSAO on a volume is all-background under software EGL even when it links. The
    frame this fixture measures is a plain volume, which does render, so a host
    that draws nothing at all skips rather than passing vacuously.
    """
    renderer = vtkOpenGLRenderer()
    renderer.AddVolume(_make_volume(shade=True))
    renderer.SetBackground(*BACKGROUND)
    _off_axis_camera(renderer)

    ren_win = vtkRenderWindow()
    ren_win.SetOffScreenRendering(1)
    ren_win.SetSize(WINDOW_SIZE, WINDOW_SIZE)
    ren_win.AddRenderer(renderer)
    try:
        ren_win.Render()
    except Exception as exc:  # pragma: no cover - host without a GL backend
        pytest.skip("could not create an OpenGL context: %s" % exc)
    if not ren_win.SupportsOpenGL():
        ren_win.Finalize()
        pytest.skip("OpenGL context is not usable on this host")

    out = vtkUnsignedCharArray()
    ren_win.GetRGBACharPixelData(0, 0, WINDOW_SIZE - 1, WINDOW_SIZE - 1, 0, out)
    pixels = vtk_to_numpy(out).reshape(WINDOW_SIZE, WINDOW_SIZE, 4)
    ren_win.Finalize()

    background = np.array([c * 255 for c in BACKGROUND] + [255], dtype=np.int16)
    covered = int(np.any(np.abs(pixels.astype(np.int16) - background) > 8, axis=2).sum())
    if covered == 0:
        pytest.skip("no volume reaches the framebuffer on this host; nothing to verify")


def test_ssao_unshaded_volume_shader_links(volume_renders):
    """An unshaded volume under SSAO must build a shader that compiles and links.

    Before the fix the substituted ``shading_gradient`` reference was undeclared
    and the fragment shader failed to compile, so the volume never reached the
    framebuffer. The "Shading must be enabled" notice proves the SSAO pass
    actually processed the volume, so the shader really was built here.
    """
    diagnostics = _render_ssao(shade=False)

    assert 'Shading must be enabled' in diagnostics, (
        "the SSAO pass did not process the volume, so nothing about its shader "
        "was exercised. Reported instead: %r" % diagnostics[:400]
    )
    for marker in _SHADER_BUILD_FAILURE:
        assert marker not in diagnostics, (
            "the unshaded volume's fragment shader failed to build under SSAO "
            "(%r in the render log); the guard on the shading_gradient reference "
            "regressed. Log: %r" % (marker, diagnostics[:400])
        )


def test_ssao_shaded_volume_shader_links(volume_renders):
    """The shaded path (which declares shading_gradient) must keep building.

    The guard's other branch leaves the shaded substitution in place, so this
    is the control that the fix did not break the previously working path.
    """
    diagnostics = _render_ssao(shade=True)

    for marker in _SHADER_BUILD_FAILURE:
        assert marker not in diagnostics, (
            "the shaded volume's fragment shader failed to build under SSAO "
            "(%r in the render log). Log: %r" % (marker, diagnostics[:400])
        )
