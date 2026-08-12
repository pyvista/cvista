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

Usage (mirrors generate_pyi.py):

    python -m cvista.generate_index -p cvista -o <outdir> vtkCommonCore ...
"""

import argparse
import importlib
import sys

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


def build_index(package, modules):
    """Map every public name to its hosting module, later-module-wins."""
    index = {}
    for module_name in modules:
        module = importlib.import_module(f'{package}.{module_name}')
        public = getattr(module, '__all__', None)
        if public is None:
            public = [name for name in vars(module) if not name.startswith('_')]
        for name in public:
            index[name] = module_name
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

    index = build_index(args.package, args.modules)
    out = f'{args.output}/_class_index.py'
    write_index(index, out)
    print(f'wrote {out}: {len(index)} names from {len(args.modules)} modules')
    return 0


if __name__ == '__main__':
    sys.exit(main())
