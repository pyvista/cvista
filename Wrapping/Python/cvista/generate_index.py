#!/usr/bin/env python3
"""Generate cvista's flat class index (``cvista/_class_index.py``).

cvista extension: PyVista (and user code) should never have to know which
internal module hosts a class -- ``from cvista import vtkPolyData`` must simply
work.  This script runs at build time (see Wrapping/Python/CMakeLists.txt),
imports every wrapped module of the build, and records each public name under
star-import semantics: the index maps ``name -> hosting module``, and when two
modules export the same name the LATER module in the given (dependency-sorted)
order wins -- exactly the name that ``from cvista.all import *`` would bind,
since ``all.py`` star-imports the modules in this same order.

Because the index is regenerated from the actual build every time, relocating a
class between internal modules (tier splits, kit moves) can never break the
flat namespace: consumers that import through it are immune to module-layout
churn by construction.

The runtime side is the module-level ``__getattr__`` in ``cvista/__init__.py``,
which resolves names through this index lazily (nothing is imported until the
first attribute access).  See also ``partition_wheels.py``, which emits the
module->tier table used to turn a missing-tier resolution into a helpful
``pip install cvista[<tier>]`` error.

Two kinds of module are indexed:

* **Wrapped (compiled) modules** -- imported, since their contents exist only
  once the extension is loaded.
* **Pure-Python helper packages** (``util``, ``numpy_interface``) -- read
  STATICALLY with ``ast``, never imported.  Those modules import third-party
  packages at module scope (numpy, and optionally xarray/GUI toolkits) that a
  wheel-build environment is not guaranteed to have; parsing keeps the index
  deterministic and identical across build environments instead of silently
  varying with whatever happened to be installed.  ``all.py`` is parsed the
  same way to find which helper names it actually binds: those, and only those,
  may override a compiled module.  Every other helper name is added only where
  the compiled modules leave a gap.

Usage (mirrors generate_pyi.py):

    python -m cvista.generate_index -p cvista -o <outdir> vtkCommonCore ...
"""

import argparse
import ast
import importlib
import sys
from pathlib import Path

# Pure-Python subpackages whose public helpers join the flat namespace. GUI
# bindings (gtk/qt/tk/wx) and the test scaffolding are deliberately excluded:
# they are not part of the computational API and pull optional toolkits.
PYTHON_HELPER_PACKAGES = ('util', 'numpy_interface')

HEADER = '''\
"""Flat class index: public name -> hosting cvista module.  GENERATED -- do not edit.

Generated at build time by cvista.generate_index from the wrapped modules of
this exact build; regenerated on every build, so it cannot drift from the
binaries it ships with.  Name collisions between modules are resolved
star-import style (later module in dependency order wins), matching
``from cvista.all import *``.
"""

INDEX = {
'''


def public_names_from_source(path):
    """Public top-level names of a pure-Python module, without importing it."""
    tree = ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
    explicit = None
    names = []      # names DEFINED here
    reexports = []  # names merely re-imported here (weaker claim -- see build_index)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            names.append(node.name)
        elif isinstance(node, ast.ImportFrom):
            # `from ... import X as Y` binds Y, and a star-import re-exports it.
            # Some helpers (util.keys) are ENTIRELY such re-exports. Restricted to
            # cvista-internal sources so third-party imports (numpy, os, ...) never
            # leak into the flat namespace.
            internal = node.level > 0 or (node.module or '').split('.')[0] == 'cvista'
            if internal:
                reexports.extend(alias.asname or alias.name for alias in node.names
                                 if alias.name != '*')
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    if target.id == '__all__':
                        explicit = [
                            e.value for e in getattr(node.value, 'elts', [])
                            if isinstance(e, ast.Constant) and isinstance(e.value, str)
                        ]
                    else:
                        names.append(target.id)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names.append(node.target.id)
    if explicit is not None:
        return explicit, []
    public = [name for name in names if not name.startswith('_')]
    return public, [name for name in reexports if not name.startswith('_')]


def all_py_helper_imports(package_dir):
    """What ``all.py`` itself binds from the pure-Python helper packages.

    Returns ``[(module_name, names_or_None), ...]`` in source order, where
    ``None`` marks a star-import.  ``None`` (rather than an empty list) is
    returned when ``all.py`` is not present, which the caller treats as "no
    helper may override a compiled module".
    """
    path = package_dir / 'all.py'
    if not path.is_file():
        return None
    tree = ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
    found = []
    for node in tree.body:
        if not isinstance(node, ast.ImportFrom) or node.level == 0 or not node.module:
            continue
        if node.module.split('.')[0] not in PYTHON_HELPER_PACKAGES:
            continue
        if any(alias.name == '*' for alias in node.names):
            found.append((node.module, None))
        else:
            found.append((node.module, [alias.asname or alias.name for alias in node.names]))
    return found


def helper_modules(package_dir):
    """Discover pure-Python helper modules, in a stable (sorted) order."""
    found = []
    for subpackage in PYTHON_HELPER_PACKAGES:
        directory = package_dir / subpackage
        if not directory.is_dir():
            continue
        for path in sorted(directory.glob('*.py')):
            if path.name == '__init__.py':
                continue
            found.append((f'{subpackage}.{path.stem}', path))
    return found


def build_index(package, modules, package_dir=None):
    """Map every public name to its hosting module, later-module-wins."""
    index = {}
    for module_name in modules:
        module = importlib.import_module(f'{package}.{module_name}')
        public = getattr(module, '__all__', None)
        if public is None:
            public = [name for name in vars(module) if not name.startswith('_')]
        for name in public:
            index[name] = module_name
    # Pure-Python helpers, parsed rather than imported (see module docstring).
    #
    # Only a helper that ``all.py`` itself imports may displace a compiled
    # module, and then only for the names all.py actually binds. all.py is the
    # definition of the flat namespace: it star-imports the compiled modules and
    # then adds three specific helper imports, so `from cvista.all import *`
    # takes VTK_DOUBLE_MAX from the compiled vtkCommonCore (1e+299) while taking
    # vtkVariantEqual from util.vtkVariant. Indexing every helper's definitions
    # as an override instead got the first of those wrong: util/vtkConstants.py
    # is a legacy pure-Python copy whose VTK_DOUBLE/FLOAT/LONG limits no longer
    # agree with the compiled headers, and it was shadowing the real values.
    #
    # Every other helper name only FILLS A GAP -- it must never displace the
    # module that already provides the name. That keeps convenience helpers
    # (numpy_to_vtk, ...) in the flat namespace without letting, say, a
    # re-export in an optional-dependency module (util.xarray_support imports
    # VTKPythonAlgorithmBase from util.vtkAlgorithm, and sorts after it) capture
    # a name and make it unresolvable whenever that dependency is absent.
    if package_dir is not None:
        parsed = {
            module_name: public_names_from_source(path)
            for module_name, path in helper_modules(package_dir)
        }
        for module_name, names in all_py_helper_imports(package_dir) or ():
            if module_name not in parsed:
                continue
            defined, reexported = parsed[module_name]
            bound = defined + reexported if names is None else names
            for name in bound:
                if not name.startswith('_'):
                    index[name] = module_name
        # Among THEMSELVES the helpers keep later-module-wins, so a name defined
        # in both util and numpy_interface resolves where it always has. Only
        # the compiled modules are now out of their reach.
        helper_defined = {}
        helper_reexported = {}
        for module_name, (defined, reexported) in parsed.items():
            for name in defined:
                helper_defined[name] = module_name
        for module_name, (defined, reexported) in parsed.items():
            for name in reexported:
                helper_reexported.setdefault(name, module_name)
        # Definitions before re-exports, so a definition still beats a mere
        # re-export when neither is claimed by all.py.
        for name, module_name in helper_defined.items():
            index.setdefault(name, module_name)
        for name, module_name in helper_reexported.items():
            index.setdefault(name, module_name)
    return index


def write_index(index, path):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HEADER)
        for name in sorted(index):
            f.write(f'    {name!r}: {index[name]!r},\n')
        f.write('}\n')


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Generate the flat class index for the cvista package.')
    parser.add_argument('-p', '--package', default='cvista',
                        help='the package the modules belong to')
    parser.add_argument('-o', '--output', required=True,
                        help='directory to write _class_index.py into')
    parser.add_argument('modules', nargs='+',
                        help='wrapped module names, in dependency order')
    args = parser.parse_args(argv)

    package_dir = Path(args.output)
    index = build_index(args.package, args.modules, package_dir)
    out = package_dir / '_class_index.py'
    write_index(index, out)
    n_helpers = len(helper_modules(package_dir))
    print(f'wrote {out}: {len(index)} names from '
          f'{len(args.modules)} wrapped + {n_helpers} pure-Python modules')
    return 0


if __name__ == '__main__':
    sys.exit(main())
