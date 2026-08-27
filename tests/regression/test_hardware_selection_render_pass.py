"""Regression: hardware selection works while a custom render pass is installed.

``vtkHardwareSelector`` captures object ids by rendering the scene with each
prop drawn in a color that encodes its id, then reading those pixels back.
``vtkOpenGLRenderer::DeviceRender`` hands the frame to ``this->Pass`` whenever a
custom render pass is installed and only reaches ``UpdateGeometry()`` -- the one
place the selector is handed the props and assigns each its id
(``vtkHardwareSelector::Render``) -- when no pass is set. A selection capture
under a pass chain therefore rendered no ids at all, and the readback came back
empty: every pick missed.

Two further consequences of the same bypass:

* ``vtkHardwareSelector::PropID`` was never initialized by the constructor and is
  only ever assigned inside that skipped loop, so
  ``vtkOpenGLHardwareSelector::BeginRenderProp()`` compared indeterminate memory
  against the 2^24-2 prop limit and reported "Too many props. Currently only
  16777214 props are supported." for scenes holding a single prop.
* ``vtkPropPicker`` and the other ``vtkRenderer::PickProp`` paths build their own
  selector internally, so they failed the same way.

The fix suspends the renderer's pass for the duration of the capture (the same
treatment multisampling already gets in
``vtkOpenGLHardwareSelector::BeginSelection``) and initializes ``PropID``.

The test drives the public API under software EGL: it selects and prop-picks a
single actor with no pass installed, then repeats with a supersampling chain and
with a Gaussian-blur chain, and asserts the results are identical to the no-pass
baseline and that the chain is still installed afterwards. It also asserts the
capture emits no VTK error, which is what the uninitialized ``PropID`` produced.
"""

import pytest

from cvista.vtkCommonCore import vtkCommand, vtkOutputWindow
from cvista.vtkCommonDataModel import vtkDataObject
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkRenderingCore import (
    vtkActor,
    vtkHardwareSelector,
    vtkPolyDataMapper,
    vtkPropPicker,
    vtkRenderer,
    vtkRenderWindow,
)
import cvista.vtkRenderingOpenGL2  # noqa: F401  (registers the OpenGL factory)
from cvista.vtkRenderingOpenGL2 import (
    vtkCameraPass,
    vtkGaussianBlurPass,
    vtkRenderPassCollection,
    vtkRenderStepsPass,
    vtkSequencePass,
    vtkSSAAPass,
)

WINDOW_SIZE = 300


def _camera_pass():
    """The stock ``vtkCameraPass`` chain every screen-space pass delegates to."""
    passes = vtkRenderPassCollection()
    passes.AddItem(vtkRenderStepsPass())
    sequence = vtkSequencePass()
    sequence.SetPasses(passes)
    camera = vtkCameraPass()
    camera.SetDelegatePass(sequence)
    return camera


def _ssaa_chain():
    """Supersampling: composites into its own enlarged framebuffer."""
    ssaa = vtkSSAAPass()
    ssaa.SetDelegatePass(_camera_pass())
    return ssaa


def _blur_chain():
    """Gaussian blur: a screen-space pass that resamples every pixel."""
    blur = vtkGaussianBlurPass()
    blur.SetDelegatePass(_camera_pass())
    return blur


def _scene():
    """A one-actor offscreen scene with a live GL context, or skip."""
    render_window = vtkRenderWindow()
    render_window.SetOffScreenRendering(1)
    render_window.SetSize(WINDOW_SIZE, WINDOW_SIZE)
    renderer = vtkRenderer()
    render_window.AddRenderer(renderer)

    sphere = vtkSphereSource()
    sphere.SetThetaResolution(32)
    sphere.SetPhiResolution(32)
    mapper = vtkPolyDataMapper()
    mapper.SetInputConnection(sphere.GetOutputPort())
    actor = vtkActor()
    actor.SetMapper(mapper)
    renderer.AddActor(actor)
    renderer.ResetCamera()
    try:
        render_window.Render()
    except Exception as exc:  # pragma: no cover - host without a GL backend
        pytest.skip("could not create an OpenGL context: %s" % exc)
    return render_window, renderer, actor


def _select(renderer, field_association):
    """Select the centre pixels; return ``(hit prop, number of selected ids)``."""
    selector = vtkHardwareSelector()
    selector.SetRenderer(renderer)
    selector.SetFieldAssociation(field_association)
    half = WINDOW_SIZE // 2
    selector.SetArea(half - 2, half - 2, half + 2, half + 2)
    selection = selector.Select()
    if selection is None or selection.GetNumberOfNodes() == 0:
        return None, 0
    node = selection.GetNode(0)
    prop = node.GetProperties().Get(node.PROP())
    ids = node.GetSelectionList()
    return prop, 0 if ids is None else ids.GetNumberOfTuples()


def _prop_pick(renderer):
    """Pick the centre pixel with ``vtkPropPicker`` (its own internal selector)."""
    picker = vtkPropPicker()
    half = WINDOW_SIZE // 2
    return bool(picker.Pick(half, half, 0, renderer))


@pytest.fixture
def vtk_errors():
    """Collect VTK error/warning events emitted during the test."""
    events = []
    window = vtkOutputWindow.GetInstance()
    tags = [
        window.AddObserver(vtkCommand.ErrorEvent, lambda *args: events.append("error")),
        window.AddObserver(vtkCommand.WarningEvent, lambda *args: events.append("warning")),
    ]
    yield events
    for tag in tags:
        window.RemoveObserver(tag)


@pytest.mark.parametrize(
    "make_chain", [_ssaa_chain, _blur_chain], ids=["ssaa", "gaussian_blur"]
)
@pytest.mark.parametrize(
    "field_association",
    [vtkDataObject.FIELD_ASSOCIATION_CELLS, vtkDataObject.FIELD_ASSOCIATION_POINTS],
    ids=["cells", "points"],
)
def test_selection_survives_render_pass(make_chain, field_association, vtk_errors):
    """A pass chain must not change what the selector and the prop picker report."""
    render_window, renderer, actor = _scene()

    baseline_prop, baseline_ids = _select(renderer, field_association)
    assert baseline_prop is actor, "the no-pass baseline did not select the actor"
    assert baseline_ids > 0
    assert _prop_pick(renderer) is True, "the no-pass baseline did not prop-pick the actor"

    chain = make_chain()
    renderer.SetPass(chain)
    render_window.Render()

    prop, ids = _select(renderer, field_association)
    assert prop is actor, "the render pass broke hardware selection"
    assert ids == baseline_ids
    assert _prop_pick(renderer) is True, "the render pass broke vtkPropPicker"

    # The capture suspends the pass; it must be restored, and the scene must
    # still render through it afterwards.
    assert renderer.GetPass() is chain
    render_window.Render()

    # An uninitialized PropID reports a bogus "Too many props" error per prop
    # rendered during the capture.
    assert vtk_errors == []
