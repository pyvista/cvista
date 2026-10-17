"""Regression coverage for vtkXOpenGLRenderWindow GLX display lifecycle and package alias imports.

Covers https://github.com/pyvista/pyvista/issues/9381:
1. Destroying the last initialized `vtkXOpenGLRenderWindow` previously closed
   `::GLADGLXLibraryDisplay` without clearing the pointer, so a subsequent
   unrealized `vtkXOpenGLRenderWindow` destructor double-closed the freed
   `Display*` and crashed with `SIGSEGV`.
2. A transient `EnsureGLX()` failure previously latched
   `GLADGLXLibraryDisplayOpened = true` while `::GLADGLXLibraryDisplay` was
   `nullptr`, permanently poisoning `EnsureGLX()` for the process lifetime.
3. Importing `vtk_module.vtk*` when `sys.modules["vtk_module"] = cvista`
   (used by `trame_vtk` under `VTK_MODULE_NAME=cvista`) must alias the
   canonical `cvista.vtk*` module rather than loading a duplicate extension.
"""

from __future__ import annotations

import importlib
import os
import shutil
import subprocess
import sys
import textwrap

import pytest

import cvista


def _find_xvfb() -> str | None:
    xvfb = shutil.which("Xvfb")
    if xvfb:
        return xvfb
    # Check common NixOS / system locations when not on PATH.
    for candidate in ("/usr/bin/Xvfb", "/usr/X11R6/bin/Xvfb"):
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    return None


@pytest.mark.skipif(sys.platform != "linux", reason="X11 render window is Linux-only")
def test_xopengl_unrealized_after_realized_window_and_retry():
    """Unrealized window teardown after a realized window must not double-close Display*."""
    from cvista import vtkRenderingOpenGL2

    if not hasattr(vtkRenderingOpenGL2, "vtkXOpenGLRenderWindow"):
        pytest.skip("Build does not include vtkXOpenGLRenderWindow")

    code = textwrap.dedent(
        """
        import gc
        import os
        import shutil
        import subprocess
        import time
        from cvista.vtkRenderingOpenGL2 import vtkXOpenGLRenderWindow
        from cvista.vtkRenderingCore import vtkRenderWindow

        xvfb_proc = None
        display = os.environ.get("DISPLAY", "")
        if not display:
            xvfb = shutil.which("Xvfb")
            if xvfb:
                display = ":109"
                xvfb_proc = subprocess.Popen(
                    [xvfb, display, "-screen", "0", "640x480x24"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                time.sleep(0.2)

        try:
            # 1. A failed EnsureGLX() on an invalid DISPLAY must not permanently
            #    latch failure or leave a corrupt Display* pointer.
            os.environ["DISPLAY"] = ""
            w_fail = vtkXOpenGLRenderWindow()
            w_fail.SetOffScreenRendering(1)
            w_fail.Initialize()
            assert not w_fail.GetInitialized()
            del w_fail
            gc.collect()

            w_unrealized = vtkXOpenGLRenderWindow()
            del w_unrealized
            gc.collect()

            if display:
                os.environ["DISPLAY"] = display
                os.environ["VTK_DEFAULT_OPENGL_WINDOW"] = "vtkXOpenGLRenderWindow"

                # 2. Realized window followed by unrealized window teardown
                #    previously double-closed ::GLADGLXLibraryDisplay (SIGSEGV).
                for _ in range(5):
                    w1 = vtkXOpenGLRenderWindow()
                    w1.SetOffScreenRendering(1)
                    w1.Initialize()
                    assert w1.GetInitialized()
                    w1.Render()
                    w1.Finalize()
                    del w1
                    gc.collect()

                    w2 = vtkXOpenGLRenderWindow()
                    del w2
                    gc.collect()

                    rw = vtkRenderWindow()
                    rw.SetOffScreenRendering(1)
                    rw.Render()
                    assert rw.GetClassName() == "vtkXOpenGLRenderWindow"
                    rw.Finalize()
                    del rw
                    gc.collect()
        finally:
            if xvfb_proc is not None:
                xvfb_proc.terminate()
                xvfb_proc.wait(timeout=5)
        """
    )
    env = os.environ.copy()
    env.pop("VTK_DEFAULT_OPENGL_WINDOW", None)
    res = subprocess.run(
        [sys.executable, "-c", code],
        env=env,
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert res.returncode == 0, f"stdout:\n{res.stdout}\nstderr:\n{res.stderr}"
    assert res.stderr.count("bad X server connection") == 1


def test_package_alias_submodule_identity(monkeypatch):
    """Submodule imports via a sys.modules package alias must resolve to canonical cvista.vtk*."""
    monkeypatch.setitem(sys.modules, "vtk_module", cvista)

    c1 = importlib.import_module("cvista.vtkCommonCore")
    c2 = importlib.import_module("vtk_module.vtkCommonCore")
    assert c1 is c2
    assert c1.__name__ == "cvista.vtkCommonCore"
    assert getattr(cvista, "vtkCommonCore") is c1
    assert not cvista.PENDING_LOADED_MODULES
