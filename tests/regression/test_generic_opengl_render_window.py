"""Regression coverage for the vtkGenericOpenGLRenderWindow wrapper."""


def test_generic_opengl_render_window_imports_and_constructs():
    from cvista import vtkGenericOpenGLRenderWindow as flat_class
    from cvista.vtkRenderingOpenGL2 import vtkGenericOpenGLRenderWindow

    assert flat_class is vtkGenericOpenGLRenderWindow

    window = vtkGenericOpenGLRenderWindow()
    assert window.GetClassName() == "vtkGenericOpenGLRenderWindow"
    assert window.GetReadyForRendering()

    window.SetReadyForRendering(False)
    assert not window.GetReadyForRendering()

    window.SetForceMaximumHardwareLineWidth(4.0)
    assert window.GetForceMaximumHardwareLineWidth() == 4.0
