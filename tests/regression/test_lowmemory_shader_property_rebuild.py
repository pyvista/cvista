"""Regression: a shader property change after the first render rebuilds the shader.

``vtkOpenGLLowMemoryPolyDataMapper::UpdateShaders`` reads whole-shader source and
every user replacement out of the actor's ``vtkShaderProperty``, but
``IsShaderUpToDate`` never compared that property's MTime. It checked the render
pass stage, the mods, the normal sources, the selection state, the PBR state and
the clipping planes, and nothing else, so a replacement added, changed or cleared
after the first render was ignored for the life of the actor: no compile was
attempted, no error was raised, and the frame did not move by a pixel.

``vtkOpenGLPolyDataMapper::GetNeedToRebuildShaders`` has always compared it, so
both mappers are parametrized here. The classic one is the control; it was
already correct, and it fails here too if someone breaks the check it has.

Each case measures against an oracle scene configured before its first render,
which by construction cannot be serving a stale program.
"""

import numpy as np
import pytest

from cvista.util.numpy_support import vtk_to_numpy
from cvista.vtkCommonCore import vtkUnsignedCharArray
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkRenderingCore import vtkActor, vtkRenderer, vtkRenderWindow
import cvista.vtkRenderingOpenGL2  # noqa: F401  (registers the OpenGL factory)
from cvista.vtkRenderingOpenGL2 import (
    vtkOpenGLLowMemoryPolyDataMapper,
    vtkOpenGLPolyDataMapper,
)

WINDOW_SIZE = 200

# The replacement re-emits the marker, so the mapper's own generated body still
# runs and the only difference is the colour forced after it. A replacement that
# swallowed the marker would change the frame by deleting the real body, which is
# not the thing under test.
COLOR_MARKER = '//VTK::Color::Impl'
FORCE_MAGENTA = """//VTK::Color::Impl
  // Plain GLSL, valid in every dialect: the assertion is about when the source
  // is rebuilt, not about what a driver will accept.
  ambientColor = vec3(1.0, 0.0, 1.0);
  diffuseColor = vec3(1.0, 0.0, 1.0);
"""

# Two frames of the same scene under the same driver are byte-identical, so a
# match is an exact comparison and this only guards the "they differ" direction.
# The magenta replacement moves about a fifth of the frame, an order of magnitude
# above it.
DIFFERENT = 0.02

# Named explicitly, never left to vtkPolyDataMapper's factory: that resolves to
# the classic mapper on a desktop build, so a factory mapper would silently test
# one of these twice.
MAPPERS = {
    'classic': vtkOpenGLPolyDataMapper,
    'lowmemory': vtkOpenGLLowMemoryPolyDataMapper,
}


def _scene(mapper_class, *, replaced):
    """A sphere, optionally carrying the replacement before its first render."""
    source = vtkSphereSource()
    source.SetThetaResolution(32)
    source.SetPhiResolution(32)

    mapper = mapper_class()
    mapper.SetInputConnection(source.GetOutputPort())

    actor = vtkActor()
    actor.SetMapper(mapper)
    actor.GetProperty().SetColor(0.85, 0.55, 0.25)
    if replaced:
        _install(actor)

    renderer = vtkRenderer()
    renderer.AddActor(actor)
    renderer.SetBackground(0.0, 0.0, 0.0)

    window = vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(WINDOW_SIZE, WINDOW_SIZE)
    # An exact frame comparison must not move with whatever multisampling the
    # driver would pick on its own.
    window.SetMultiSamples(0)
    window.AddRenderer(renderer)
    return window, actor


def _install(actor):
    actor.GetShaderProperty().AddFragmentShaderReplacement(
        COLOR_MARKER,
        True,  # replaceFirst
        FORCE_MAGENTA,
        False,  # replaceAll
    )


def _clear(actor):
    actor.GetShaderProperty().ClearFragmentShaderReplacement(COLOR_MARKER, True)


def _render(window):
    """Render and read the frame back, or skip if this host has no GL."""
    try:
        window.Render()
    except Exception as exc:  # pragma: no cover - host without a GL backend
        pytest.skip('could not create an OpenGL context: %s' % exc)
    if not window.SupportsOpenGL():
        pytest.skip('OpenGL context is not usable on this host')
    pixels = vtkUnsignedCharArray()
    window.GetPixelData(0, 0, WINDOW_SIZE - 1, WINDOW_SIZE - 1, 0, pixels, 0)
    return vtk_to_numpy(pixels).astype(np.int16).reshape(-1, 3)


def _changed_fraction(a, b):
    return float(np.mean(np.any(a != b, axis=1)))


def _oracle(mapper_class, *, replaced):
    """The frame a scene configured this way from birth produces."""
    window, _ = _scene(mapper_class, replaced=replaced)
    frame = _render(window)
    window.Finalize()
    return frame


@pytest.mark.parametrize('mapper_name', sorted(MAPPERS))
def test_replacement_added_after_the_first_render_reaches_the_program(mapper_name):
    mapper_class = MAPPERS[mapper_name]
    oracle = _oracle(mapper_class, replaced=True)

    window, actor = _scene(mapper_class, replaced=False)
    before = _render(window)
    _install(actor)
    after = _render(window)
    window.Finalize()

    assert _changed_fraction(after, before) > DIFFERENT, (
        'adding a fragment shader replacement after the first render moved '
        'nothing: the shader was not rebuilt'
    )
    assert np.array_equal(after, oracle), (
        'the late replacement rendered something other than what the same '
        'replacement installed before the first render renders'
    )


@pytest.mark.parametrize('mapper_name', sorted(MAPPERS))
def test_replacement_cleared_after_the_first_render_reaches_the_program(mapper_name):
    # The clear direction is a separate failure: a stale program keeps rendering
    # the replacement long after the caller took it away.
    mapper_class = MAPPERS[mapper_name]
    oracle = _oracle(mapper_class, replaced=False)

    window, actor = _scene(mapper_class, replaced=True)
    before = _render(window)
    _clear(actor)
    after = _render(window)
    window.Finalize()

    assert _changed_fraction(after, before) > DIFFERENT, (
        'clearing a fragment shader replacement after the first render moved '
        'nothing: the shader was not rebuilt'
    )
    assert np.array_equal(after, oracle), (
        'clearing the replacement did not restore the frame an actor that never '
        'carried it renders'
    )


@pytest.mark.parametrize('mapper_name', sorted(MAPPERS))
def test_an_unchanged_shader_property_does_not_force_a_rebuild(mapper_name):
    # The other direction of the same check: keying on an MTime that something
    # bumps every frame would recompile the program on every frame, which no
    # pixel assertion above would notice. Reading the shader property must not
    # be what bumps it.
    mapper_class = MAPPERS[mapper_name]
    window, actor = _scene(mapper_class, replaced=True)
    _render(window)
    mtime = actor.GetShaderProperty().GetShaderMTime()
    _render(window)
    window.Finalize()

    assert actor.GetShaderProperty().GetShaderMTime() == mtime, (
        'rendering bumped the shader property MTime, so the rebuild check it '
        'feeds would fire on every frame'
    )
