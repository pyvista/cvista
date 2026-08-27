"""Regression: the low-memory mapper never draws with a shader program that failed to build.

``vtkOpenGLLowMemoryPolyDataMapper::RenderPieceDraw`` called
``SetShaderParameters`` and then the primitive agents on whatever
``ReadyShaderProgram`` had left in ``ShaderProgram``, which is nullptr whenever
the program does not compile, link or bind. ``SetShaderParameters`` checks for
null and returns; the agents do not, so
``vtkOpenGLLowMemoryCellTypeAgent::PreDraw`` sets ``enable_lights`` through a
null ``vtkShaderProgram*`` and the process takes SIGSEGV. Without the guard
these tests do not fail, they crash the interpreter and take the rest of the
session's tests with them.

There is GL state damage underneath the crash as well: a failed build never
reaches ``vtkOpenGLShaderCache``'s ``BindShader``, so the program left current
belongs to the previous prop and a draw issued now is issued against that.

No stale-program half is involved on this mapper. ``ReadyShaderProgram`` runs on
every render and assigns its result unconditionally, so a build that fails after
a good frame nulls the program rather than leaving the working one from the
first frame in place.

The mapper is always named explicitly here. ``vtkPolyDataMapper``'s factory
resolves to the classic mapper on a desktop build, so a factory mapper would
silently test the wrong class, and this suite runs on desktop software EGL. A
fragment shader replacement that does not compile reproduces a failed build on
any driver, which is also the only form of it llvmpipe can run.
"""

import numpy as np
import pytest

from cvista.util.numpy_support import vtk_to_numpy
from cvista.vtkCommonCore import vtkOutputWindow, vtkStringOutputWindow, vtkUnsignedCharArray
from cvista.vtkCommonDataModel import vtkPolyData
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkRenderingCore import vtkActor, vtkRenderer, vtkRenderWindow
import cvista.vtkRenderingOpenGL2  # noqa: F401  (registers the OpenGL factory)
from cvista.vtkRenderingOpenGL2 import vtkOpenGLLowMemoryPolyDataMapper

WINDOW_SIZE = 200

# The renderer's clear colour, and how far a channel has to move from it before
# the pixel counts as the actor. Nothing here is anti-aliased against a gradient,
# so the count is stable well inside this margin.
BACKGROUND = (0.0, 0.0, 0.0)
PIXEL_TOLERANCE = 8

# The sphere below covers about a quarter of the frame. The thresholds are a
# floor and a ceiling around that rather than a tuned value: what is asserted is
# "the actor is there" versus "the actor is not there", and the failure mode
# being guarded against moves this number between 0.0 and roughly 0.25.
DREW = 0.05
DREW_NOTHING = 0.001

# The marker is re-emitted, so the mapper's own generated body survives and the
# program can only fail for the planted reason. User replacements are applied
# before the mapper's own, so a replacement that swallowed the marker would
# delete the real body and fail for that instead.
COLOR_MARKER = '//VTK::Color::Impl'
INVALID_FRAGMENT = """//VTK::Color::Impl
  // Not GLSL in any dialect, deliberately: the assertion is about a program
  // that cannot be built, and every driver has to agree that this one cannot.
  this_type_does_not_exist deliberately_invalid = ;
"""


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


def _scene():
    """A one-sphere offscreen scene drawn by the low-memory mapper, named explicitly.

    Raises if the host has no usable GL context; only the fixture turns that
    into a skip.
    """
    window = vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(WINDOW_SIZE, WINDOW_SIZE)
    # The assertion is a pixel count against the clear colour, which must not
    # move with whatever multisampling the driver would pick on its own.
    window.SetMultiSamples(0)
    renderer = vtkRenderer()
    renderer.SetBackground(*BACKGROUND)
    window.AddRenderer(renderer)

    sphere = vtkSphereSource()
    sphere.SetThetaResolution(32)
    sphere.SetPhiResolution(32)
    mapper = vtkOpenGLLowMemoryPolyDataMapper()
    mapper.SetInputConnection(sphere.GetOutputPort())
    actor = vtkActor()
    actor.SetMapper(mapper)
    actor.GetProperty().SetColor(1.0, 0.6, 0.2)
    # Ambient carries the colour on its own, so the actor is unmistakably
    # present even where the single headlight does not reach it.
    actor.GetProperty().SetAmbient(0.6)
    actor.GetProperty().SetDiffuse(0.4)
    renderer.AddActor(actor)
    renderer.ResetCamera()
    return window, actor


def _coverage(window):
    """Fraction of the frame that is not the clear colour."""
    pixels = vtkUnsignedCharArray()
    window.GetPixelData(0, 0, WINDOW_SIZE - 1, WINDOW_SIZE - 1, 0, pixels, 0)
    rgb = vtk_to_numpy(pixels).astype(np.int16)
    background = np.array([channel * 255.0 for channel in BACKGROUND], dtype=np.int16)
    return float(np.mean(np.any(np.abs(rgb - background) > PIXEL_TOLERANCE, axis=1)))


def _poison(actor):
    actor.GetShaderProperty().AddFragmentShaderReplacement(
        COLOR_MARKER, True, INVALID_FRAGMENT, False
    )


@pytest.fixture
def healthy_coverage():
    """What this scene covers on this host when nothing is wrong with it.

    Every assertion below is "the actor is missing" and would hold vacuously on
    a host that renders nothing at all, so the reference frame is measured
    rather than assumed, and the whole module skips if it is empty.
    """
    try:
        window, _actor = _scene()
        window.Render()
    except Exception as exc:  # pragma: no cover - host without a GL backend
        pytest.skip('could not create an OpenGL context: %s' % exc)
    if not window.SupportsOpenGL():
        window.Finalize()
        pytest.skip('OpenGL context is not usable on this host')
    covered = _coverage(window)
    window.Finalize()
    if covered <= DREW:
        pytest.skip('the reference frame is empty (%.3f covered); no usable renderer' % covered)
    return covered


def test_a_program_that_never_built_is_not_drawn_with(healthy_coverage):
    """An actor whose shader never compiled must draw nothing and say why.

    The null half of the guard, on an actor that carries the bad replacement
    from birth. This is the case that segfaults without the guard, in the agents
    reached from ``RenderPieceDraw``.
    """
    window, actor = _scene()
    try:
        _poison(actor)
        with _Diagnostics() as diagnostics:
            window.Render()
        after = _coverage(window)
    finally:
        window.Finalize()

    assert 'Could not set shader program' in diagnostics.text, (
        'the mapper drew, or declined to draw, without saying why; a caller '
        'cannot tell a failed build from an empty input by looking at the frame. '
        'Reported instead: %r' % diagnostics.text[:400]
    )
    assert after < DREW_NOTHING, (
        'the actor covered %.3f of the frame after its shader program failed to '
        'build; the same scene covers %.3f when it compiles'
        % (after, healthy_coverage)
    )


def test_a_program_that_stopped_building_is_not_drawn_with(healthy_coverage):
    """Breaking the shader after a good frame must stop the actor, not the process.

    This case only exists because the mapper now rebuilds on a
    ``vtkShaderProperty`` change. Before that, a replacement installed on an
    already-rendered actor was never compiled, so it could never fail, and the
    null program was only reachable on an actor poisoned from birth. That fix
    trades a silent no-op for a reachable crash, and this guard is what makes
    the trade sound, so this is the test that fails if the two are separated.

    One window before and after, so the replacement is the only variable.
    """
    del healthy_coverage  # taken for the host check only

    window, actor = _scene()
    try:
        window.Render()
        before = _coverage(window)
        assert before > DREW, (
            'the low-memory mapper covered only %.3f of the frame before anything '
            'was planted, so the assertion below would hold vacuously' % before
        )

        with _Diagnostics() as diagnostics:
            _poison(actor)
            window.Render()
        after = _coverage(window)
    finally:
        window.Finalize()

    assert 'Could not set shader program' in diagnostics.text, (
        'the late replacement was never compiled, so this test proves nothing '
        'about a program that failed to build. Reported instead: %r'
        % diagnostics.text[:400]
    )
    assert after < DREW_NOTHING, (
        'the actor covered %.3f of the frame (was %.3f) after a replacement '
        'installed on it failed to compile' % (after, before)
    )


def test_the_mapper_recovers_once_the_shader_compiles_again(healthy_coverage):
    """One failed build must not leave the actor invisible forever.

    Returning early from ``RenderPieceDraw`` is only safe because
    ``IsShaderUpToDate`` returns false immediately on a null ``ShaderProgram``,
    so the next render rebuilds from the current sources instead of skipping the
    build and hitting the same guard again. That ordering is what this pins.
    """
    window, actor = _scene()
    try:
        _poison(actor)
        with _Diagnostics():
            window.Render()
        assert _coverage(window) < DREW_NOTHING, 'the poisoned frame drew the actor anyway'

        with _Diagnostics() as diagnostics:
            actor.GetShaderProperty().ClearFragmentShaderReplacement(COLOR_MARKER, True)
            window.Render()
        recovered = _coverage(window)
    finally:
        window.Finalize()

    assert 'Could not set shader program' not in diagnostics.text, (
        'the mapper never rebuilt after the replacement was removed: %r'
        % diagnostics.text[:400]
    )
    assert recovered > DREW, (
        'the actor covered %.3f of the frame after the bad replacement was removed, '
        'against %.3f before it was ever added: one failed build left the mapper '
        'unable to rebuild' % (recovered, healthy_coverage)
    )


def test_a_failed_build_and_nothing_to_draw_do_not_look_the_same(healthy_coverage):
    """An empty frame has to say which kind of empty it is.

    Both of these put no pixels on the screen, and the point of guarding the
    draw rather than letting it crash is that the guard must not turn a crash
    into a silent no-op. The failed build reports; a mapper with no points does
    not, because there is nothing wrong with it. If this ever goes red because
    the two agree, the guard has become the silent failure it replaced.

    Here for the contract rather than for the mutation: deleting the guard
    leaves this green, because a mapper with no points returns from
    ``RenderPiece`` before ``RenderPieceDraw`` is reached. The silent half is
    caught by the error assertion in
    ``test_a_program_that_never_built_is_not_drawn_with``; this pins the other
    direction, which nothing else would notice.
    """
    del healthy_coverage  # taken for the host check only

    window, actor = _scene()
    try:
        actor.GetMapper().SetInputData(vtkPolyData())
        with _Diagnostics() as quiet:
            window.Render()
        empty_coverage = _coverage(window)
    finally:
        window.Finalize()

    assert empty_coverage < DREW_NOTHING, (
        'a mapper with no points covered %.3f of the frame, so this test is not '
        'measuring an empty frame at all' % empty_coverage
    )
    assert 'Could not set shader program' not in quiet.text, (
        'having nothing to draw was reported as a failed shader build, which '
        'makes the report useless for telling the two apart: %r' % quiet.text[:400]
    )
