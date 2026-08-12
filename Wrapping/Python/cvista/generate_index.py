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
  varying with whatever happened to be installed.  They are indexed AFTER the
  compiled modules, matching the order ``all.py`` star-imports them in.

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
    # A name DEFINED in a helper follows the same later-wins rule as above, but a
    # mere RE-EXPORT only fills a gap: it must never displace the module that
    # actually defines the name. Without that rule a re-export in an
    # optional-dependency module (util.xarray_support imports VTKPythonAlgorithmBase
    # from util.vtkAlgorithm, and sorts after it) would capture the name and make it
    # unresolvable whenever that optional dependency is absent.
    if package_dir is not None:
        pending_reexports = []
        for module_name, path in helper_modules(package_dir):
            defined, reexported = public_names_from_source(path)
            for name in defined:
                index[name] = module_name
            pending_reexports.append((module_name, reexported))
        for module_name, reexported in pending_reexports:
            for name in reexported:
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
