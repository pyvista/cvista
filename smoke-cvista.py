#!/usr/bin/env python3
"""Native cvista smoke test — validates the `cvista` package (post vtkmodules rename).

cvista installs as the `cvista` import package with dist name `cvista`, so the stock-PyVista
parity suite (which imports `vtkmodules`) cannot drive it unpatched. This exercises the
renamed package directly: top-level import, the kit submodules, the util/numpy_interface
shims, a numpy<->vtk roundtrip, a filter pipeline, and an EGL offscreen render.

Run inside the build's nix-shell so EGL/OSMesa resolve, e.g.:

    pip install dist/cvista-*.whl
    VTK_DEFAULT_OPENGL_WINDOW=vtkEGLRenderWindow VTK_EGL_DEVICE_INDEX=0 \
        python smoke-cvista.py

Exit code 0 = all pass.
"""
import importlib.util
import os
import sys

FAILS = []


def check(name, fn):
    try:
        fn()
        print(f"  ok   {name}", flush=True)
    except Exception as e:  # noqa: BLE001
        print(f"  FAIL {name}: {type(e).__name__}: {e}", flush=True)
        FAILS.append(name)


print("=== cvista namespace smoke ===", flush=True)

def _toplevel():
    import cvista  # noqa: F401
    assert importlib.util.find_spec("vtkmodules") is None, "stray vtkmodules present"
check("import cvista (no stray vtkmodules)", _toplevel)

def _version():
    from cvista.vtkCommonCore import vtkVersion
    print("       VTK_VERSION =", vtkVersion.GetVTKVersion(), flush=True)
check("cvista.vtkCommonCore.vtkVersion", _version)

def _kits():
    from cvista import (  # noqa: F401
        vtkCommonCore, vtkCommonDataModel, vtkCommonExecutionModel,
        vtkFiltersCore, vtkFiltersGeometry, vtkRenderingCore,
    )
check("from cvista import <kits>", _kits)

def _instantiate():
    from cvista.vtkCommonDataModel import vtkPolyData
    mod = type(vtkPolyData()).__module__
    assert mod.startswith("cvista") and "vtkmodules" not in mod, f"type module: {mod}"
check("instantiate vtkPolyData (cvista.* module)", _instantiate)

def _util():
    import numpy as np
    from cvista.util.numpy_support import numpy_to_vtk, vtk_to_numpy
    a = np.arange(30, dtype=np.float64).reshape(10, 3)
    assert np.allclose(a, vtk_to_numpy(numpy_to_vtk(a))), "numpy roundtrip mismatch"
check("cvista.util.numpy_support roundtrip", _util)

def _dsa():
    from cvista.numpy_interface import dataset_adapter as dsa  # noqa: F401
check("cvista.numpy_interface.dataset_adapter", _dsa)

def _flat():
    # Flat lazy namespace: class access with no knowledge of the hosting module.
    from cvista import vtkPolyData, vtkSphereSource, vtkXMLPolyDataReader  # noqa: F401
    import cvista
    from cvista.vtkCommonDataModel import vtkPolyData as direct
    assert cvista.vtkPolyData is direct, "flat resolve != direct module import"
    assert "vtkPolyData" in dir(cvista), "flat names missing from dir(cvista)"
    try:
        cvista.vtkDoesNotExist
    except AttributeError:
        pass
    else:
        raise AssertionError("unknown flat name did not raise AttributeError")
check("flat namespace (from cvista import vtkPolyData)", _flat)

def _flat_index_complete():
    # Every name hosted by a WRAPPED (compiled) module must resolve on a full
    # install -- that is the guarantee the flat namespace makes, and this catches
    # index drift. Pure-Python helpers (dotted names: util.*, numpy_interface.*)
    # are only reported: they carry optional third-party deps (numpy, and
    # xarray/cftime for util.xarray_support) that need not be present. The split
    # keys on the hosting module rather than the error text, because
    # util.pickle_support swallows its own numpy failure and re-raises a bare
    # ImportError with no `name` set.
    import cvista
    from cvista import _class_index
    hard, soft = [], []
    for name, module in _class_index.INDEX.items():
        try:
            getattr(cvista, name)
        except Exception as e:  # noqa: BLE001
            (soft if "." in module else hard).append((name, module, repr(e)[:120]))
    if soft:
        print(f"       {len(soft)} helper name(s) unavailable, optional deps "
              f"not installed (OK): {sorted({m for _, m, _ in soft})}", flush=True)
    assert not hard, f"{len(hard)} indexed names failed, first 5: {hard[:5]}"
    print(f"       flat index: {len(_class_index.INDEX) - len(soft)} names resolve", flush=True)
check("flat index exhaustive resolve", _flat_index_complete)

def _filter():
    from cvista.vtkFiltersSources import vtkSphereSource
    from cvista.vtkFiltersCore import vtkTriangleFilter
    s = vtkSphereSource(); s.SetThetaResolution(16); s.SetPhiResolution(16)
    t = vtkTriangleFilter(); t.SetInputConnection(s.GetOutputPort()); t.Update()
    n = t.GetOutput().GetNumberOfCells()
    assert n > 0, "no cells out of sphere->triangle pipeline"
    print("       sphere->triangle cells =", n, flush=True)
check("filter pipeline (sphere->triangle)", _filter)

def _render():
    os.environ.setdefault("VTK_DEFAULT_OPENGL_WINDOW", "vtkEGLRenderWindow")
    os.environ.setdefault("VTK_EGL_DEVICE_INDEX", "0")
    from cvista.vtkFiltersSources import vtkConeSource
    from cvista.vtkRenderingCore import (
        vtkRenderer, vtkRenderWindow, vtkPolyDataMapper, vtkActor)
    import cvista.vtkRenderingOpenGL2  # noqa: F401  (registers GL factory)
    cone = vtkConeSource()
    m = vtkPolyDataMapper(); m.SetInputConnection(cone.GetOutputPort())
    a = vtkActor(); a.SetMapper(m)
    ren = vtkRenderer(); ren.AddActor(a)
    rw = vtkRenderWindow(); rw.SetOffScreenRendering(1); rw.AddRenderer(ren)
    rw.SetSize(150, 150); rw.Render()
    print("       offscreen render OK; window =", rw.GetClassName(), flush=True)
check("EGL offscreen render", _render)

print(f"=== {'ALL PASS' if not FAILS else 'FAILURES: ' + ', '.join(FAILS)} ===", flush=True)
sys.exit(1 if FAILS else 0)
