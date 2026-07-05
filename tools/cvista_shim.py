"""Validation shim: redirect `vtkmodules[.*]` imports to `cvista[.*]`.

Runs at interpreter startup (sitecustomize is auto-imported), so the redirect is
active before pyvista's own conftest/_vtk lazy-loader imports anything. Stock
`vtk`/`vtkmodules` are UNINSTALLED in this venv, so any import that slips past the
redirect fails loudly (ModuleNotFoundError) instead of silently testing stock VTK
-- we want a loud miss, never a false green.

Unlike the README's minimal find_spec shim, this registers the resolved cvista
module under the *requested* vtkmodules name (aliases in sys.modules), which the
full pyvista test suite needs (importlib.import_module caches by requested name).

cvista relocates a handful of classes into different modules than stock VTK (to
keep some modules rendering-free / to split wheel tiers). Stock VTK -- and any
consumer written against it, including pyvista -- still expects them under their
ORIGINAL module name (e.g. `from vtkmodules.vtkFiltersHybrid import
vtkPolyDataSilhouette`). To preserve that drop-in import contract, after
resolving a redirected module we re-export any classes cvista moved out of it
back onto the module object, pulled from their new home. See _RELOCATED below;
NOWRAP classes (absent as Python attributes) are silently skipped.
"""
import importlib
import importlib.abc
import importlib.util
import sys

# Original stock-VTK module -> [(cvista's new module, (relocated class names, ...))].
# Mirrors the C++ module moves in cvista (FiltersHybrid -> FiltersHybridRendering,
# IOGeometry -> IOImport). Keep in sync when a class is relocated.
_RELOCATED = {
    "vtkFiltersHybrid": [
        (
            "vtkFiltersHybridRendering",
            (
                "vtkPolyDataSilhouette",
                "vtkRenderLargeImage",
                "vtkAdaptiveDataSetSurfaceFilter",
            ),
        ),
    ],
    "vtkIOGeometry": [
        ("vtkIOImport", ("vtkGLTFReader", "vtkGLTFTexture")),
    ],
    "vtkImagingHybrid": [
        ("vtkImagingHybridIO", ("vtkSliceCubes",)),
    ],
}


class _CvistaRedirect(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    PREFIX = "vtkmodules"
    TARGET = "cvista"

    def find_spec(self, name, path=None, target=None):
        if name == self.PREFIX or name.startswith(self.PREFIX + "."):
            return importlib.util.spec_from_loader(name, self)
        return None

    def create_module(self, spec):
        suffix = spec.name[len(self.PREFIX):]  # e.g. ".vtkFiltersHybrid" or ""
        target = self.TARGET + suffix
        mod = importlib.import_module(target)
        self._reexport_relocated(mod, suffix.lstrip("."))
        sys.modules[spec.name] = mod  # alias under the requested vtkmodules name
        return mod

    def _reexport_relocated(self, mod, bare_name):
        """Graft cvista-relocated classes back onto their original module."""
        for new_module, class_names in _RELOCATED.get(bare_name, ()):
            try:
                src = importlib.import_module(self.TARGET + "." + new_module)
            except ModuleNotFoundError:
                continue
            for class_name in class_names:
                if not hasattr(mod, class_name) and hasattr(src, class_name):
                    try:
                        setattr(mod, class_name, getattr(src, class_name))
                    except (AttributeError, TypeError):
                        pass

    def exec_module(self, module):
        pass  # already executed by import_module in create_module


if not any(isinstance(f, _CvistaRedirect) for f in sys.meta_path):
    sys.meta_path.insert(0, _CvistaRedirect())
