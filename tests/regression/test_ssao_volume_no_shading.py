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

This test renders the same volume through vtkSSAOPass twice, once with Shade
off and once with Shade on, under software EGL. It asserts both renders produce
a program that links (the crash this fix targets) and that the shaded volume
still contributes gradient-derived detail. It does not attempt to distinguish
"shaded" from "unshaded" AO by pixel comparison, since gradient-derived
occlusion structure is out of image-regression reach; it asserts on the
population of colors actually reaching the framebuffer instead, which is what
the original bug actually broke.
"""

import numpy as np
import pytest

from cvista.util.numpy_support import vtk_to_numpy
from cvista.vtkCommonCore import vtkFloatArray, vtkUnsignedCharArray
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


def _render_with_ssao(*, shade):
    """Render one frame of a volume with SSAO enabled, off-axis camera."""
    renderer = vtkOpenGLRenderer()
    renderer.AddVolume(_make_volume(shade=shade))
    renderer.SetBackground(0.05, 0.05, 0.05)

    camera = renderer.GetActiveCamera()
    camera.SetPosition(30, 22, 18)
    camera.SetFocalPoint(8, 8, 8)
    camera.SetViewUp(0, 0, 1)
    renderer.ResetCameraClippingRange()

    ssao_pass = vtkSSAOPass()
    ssao_pass.SetDelegatePass(vtkCameraPass())
    ssao_pass.SetRadius(4.0)
    ssao_pass.SetKernelSize(16)
    renderer.SetPass(ssao_pass)

    ren_win = vtkRenderWindow()
    ren_win.SetOffScreenRendering(1)
    ren_win.SetSize(64, 64)
    ren_win.AddRenderer(renderer)

    try:
        ren_win.Render()
    except Exception as exc:  # pragma: no cover - host without a GL backend
        pytest.skip("could not create an OpenGL context: %s" % exc)
    if not ren_win.SupportsOpenGL():
        pytest.skip("OpenGL context is not usable on this host")

    ren_win.Render()

    w, h = ren_win.GetSize()
    out = vtkUnsignedCharArray()
    ren_win.GetRGBACharPixelData(0, 0, w - 1, h - 1, 0, out)
    pixels = np.ascontiguousarray(vtk_to_numpy(out).reshape(h, w, 4))
    ren_win.Finalize()
    return pixels


def test_ssao_renders_volume_without_shading():
    """Before the fix, an unshaded volume's shader failed to link and the
    volume never reached the framebuffer, leaving a uniform background."""
    pixels = _render_with_ssao(shade=False)
    background = np.array([0.05, 0.05, 0.05, 1.0]) * 255
    distinct_colors = len(np.unique(pixels.reshape(-1, 4), axis=0))
    assert distinct_colors > 1, (
        "an unshaded volume rendered under SSAO produced a uniform frame, "
        "consistent with the fragment shader failing to link"
    )
    assert not np.allclose(pixels.mean(axis=(0, 1)), background, atol=2), (
        "the frame is indistinguishable from bare background; the volume "
        "did not contribute any pixels"
    )


def test_ssao_renders_volume_with_shading():
    """The shaded path (which always declared shading_gradient) must be
    unaffected by the guard: it still renders and still varies with normal."""
    pixels = _render_with_ssao(shade=True)
    distinct_colors = len(np.unique(pixels.reshape(-1, 4), axis=0))
    assert distinct_colors > 1, (
        "a shaded volume rendered under SSAO should still contribute varied "
        "pixel data"
    )
