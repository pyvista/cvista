"""Validation shim: redirect `vtkmodules[.*]` and legacy `vtk` imports to `cvista[.*]`.

Runs at interpreter startup (sitecustomize is auto-imported), so the redirect is
active before pyvista's own conftest/_vtk lazy-loader imports anything.

PyVista itself is installed --no-deps precisely so it cannot drag stock `vtk` in,
but a transitive dependency of the test group still can (trame-pyvista pulls the
trame stack). So do NOT assume stock VTK is absent from this venv: assume only
that this finder sits at sys.meta_path[0] and therefore WINS. It claims every
`vtkmodules*` name AND the top-level `vtk` name, so a stock install sitting in
site-packages is shadowed rather than silently exercised -- the point being a
loud miss, never a false green.

The legacy `vtk` interception matters because stock's `vtk.py` is an eager
star-import of every vtkmodules module. Left unshadowed, one `import vtk` from a
consumer (trame-vtk's serializers do exactly this) drags in module names the fork
deliberately trims and dies on the first one. See _LegacyVtkModule.

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
import types

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


class _LegacyVtkModule(types.ModuleType):
    """Stand-in for stock VTK's top-level monolithic ``vtk`` module.

    Legacy consumers still do a bare ``import vtk`` and reach for
    ``vtk.vtkPolyData`` -- trame-vtk's serializers are one
    (``trame_vtk/modules/vtk/serializers/serialize.py``). Stock's ``vtk.py`` is
    an eager ``from vtkmodules.<every module> import *``, which cannot work
    against a trimmed fork: it names modules cvista deliberately does not build
    (``vtkViewsInfovis``, ...), so the redirect below would raise
    ``ModuleNotFoundError`` for a module the caller never actually wanted.

    Resolving lazily through cvista's flat namespace instead gives the same
    ``vtk.vtkFoo`` surface without eagerly importing anything, and a class the
    fork does not ship fails only if someone actually asks for it.
    """

    def __getattr__(self, name):
        # NEVER forward dunders to cvista. The import machinery probes
        # `getattr(module, '__package__', None)` and only initializes the
        # attribute when that returns None; forwarding handed it cvista's own
        # '__package__' ('cvista', since cvista IS a package), so `vtk` ended up
        # with __package__='cvista' against __spec__.parent='' and every
        # subsequent import raised "DeprecationWarning: __package__ !=
        # __spec__.parent" -- fatal under PyVista's filterwarnings=error.
        # Same reasoning covers __path__, __all__, __file__ and friends.
        if name.startswith('__') and name.endswith('__'):
            raise AttributeError(name)

        import cvista

        try:
            return getattr(cvista, name)
        except ImportError as exc:  # trimmed-away module: report it as absent
            raise AttributeError(str(exc)) from exc
        except AttributeError:
            # Older cvista wheels predate the flat namespace (no _class_index),
            # so nothing resolves off the package root. Fall back to the eager
            # `cvista.all` aggregate, which every build ships.
            all_module = importlib.import_module("cvista.all")
            return getattr(all_module, name)

    def __dir__(self):
        import cvista

        names = set(dir(cvista))
        try:
            names.update(dir(importlib.import_module("cvista.all")))
        except ImportError:
            pass
        return sorted(names)


class _CvistaRedirect(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    PREFIX = "vtkmodules"
    TARGET = "cvista"
    LEGACY = "vtk"
    IDENTITY = "__cvista_true_identity__"

    def find_spec(self, name, path=None, target=None):
        if name == self.PREFIX or name.startswith(self.PREFIX + "."):
            return importlib.util.spec_from_loader(name, self)
        if name == self.LEGACY:
            return importlib.util.spec_from_loader(name, self)
        return None

    def create_module(self, spec):
        if spec.name == self.LEGACY:
            mod = _LegacyVtkModule(self.LEGACY)
            # Set explicitly rather than relying on _init_module_attrs: `vtk` is a
            # top-level module, so its package is '' (see __getattr__ above).
            mod.__package__ = ''
            sys.modules[spec.name] = mod
            return mod
        suffix = spec.name[len(self.PREFIX):]  # e.g. ".vtkFiltersHybrid" or ""
        target = self.TARGET + suffix
        mod = importlib.import_module(target)
        self._reexport_relocated(mod, suffix.lstrip("."))
        # Aliasing means ONE module object is reachable under two names, and the
        # import machinery unconditionally stamps the requested spec onto whatever
        # create_module returns -- rewriting the real cvista module's __name__ /
        # __spec__ / __package__ to the vtkmodules ones. That corrupts the genuine
        # module: its __package__ ('cvista') then disagrees with its __spec__.parent
        # (''), and every import performed from cvista's own globals raises
        # "DeprecationWarning: __package__ != __spec__.parent" -- fatal under
        # PyVista's filterwarnings=error. Stash the true identity here and restore
        # it in exec_module, which runs after the machinery has stamped the module.
        mod.__dict__.setdefault(
            self.IDENTITY,
            (mod.__name__, mod.__spec__, getattr(mod, "__package__", None)),
        )
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
        # Already executed by import_module in create_module. Runs after
        # _init_module_attrs stamped the requested (vtkmodules) spec onto the
        # shared object, so this is where the real cvista identity goes back.
        identity = module.__dict__.get(self.IDENTITY)
        if identity is not None:
            module.__name__, module.__spec__, module.__package__ = identity


if not any(isinstance(f, _CvistaRedirect) for f in sys.meta_path):
    sys.meta_path.insert(0, _CvistaRedirect())
