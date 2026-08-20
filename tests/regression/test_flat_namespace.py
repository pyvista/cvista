#!/usr/bin/env python
"""Regression: the flat namespace must agree with ``cvista.all``, object by object.

``all.py`` is what ``from cvista import X`` is meant to reproduce.  It
star-imports the wrapped modules in dependency order and then binds exactly
three things from the pure-Python helpers: ``vtkImageScalarTypeNameMacro``,
``calldata_type``, and ``from .util.vtkVariant import *``.  Anything else a
helper happens to define is auxiliary and must not displace a compiled module.

9.6.2.4 got that wrong.  ``build_index()`` indexed every name a helper defines
as an override, so ``util/vtkConstants.py`` -- a legacy pure-Python copy whose
numbers have drifted from the headers -- captured six constants:

    from cvista import VTK_DOUBLE_MAX   ->  1e+99    (compiled: 1e+299)
    from cvista import VTK_LONG_MAX     ->  2**31-1  (compiled: 2**63-1)

Those resolve perfectly well, they are simply wrong, which is why the
exhaustive-resolve check in smoke-cvista.py was happy with them.  Comparing
values against ``cvista.all`` is the check that catches it, so it belongs in CI
rather than in a script someone runs by hand.
"""

import importlib

import pytest

# Stdlib and __future__ names that leak into stock VTK's ``all`` namespace via
# star-import.  The index deliberately does not carry them.
NOT_INDEXED = frozenset({'sys', 'absolute_import'})

# A wrapped module that hosts a class of its own name wins over the class: PEP
# 562's __getattr__ only runs when normal attribute lookup fails, and importing
# cvista.vtkWebGLExporter binds the MODULE onto the package permanently.
# Resolving that in favor of the class means intercepting attribute access on
# the package itself; until that is decided, it is pinned here so the day it
# changes is a deliberate one.
MODULE_SHADOWS_CLASS = frozenset({'vtkWebGLExporter'})


def _all_names():
    import cvista.all as vtkall

    return vtkall, [name for name in dir(vtkall) if not name.startswith('_')]


def test_flat_namespace_matches_all():
    import cvista

    vtkall, names = _all_names()
    assert len(names) > 2000, f'cvista.all looks truncated: {len(names)} names'

    wrong = []
    missing = []
    for name in names:
        want = getattr(vtkall, name)
        try:
            got = getattr(cvista, name)
        except AttributeError:
            missing.append(name)
            continue
        if got is want:
            continue
        try:
            same = bool(got == want)
        except Exception:  # noqa: BLE001  (a wrapped __eq__ may raise)
            same = False
        if not same:
            wrong.append(f'{name}: flat={got!r} all={want!r}')

    unexpected_wrong = [w for w in wrong if w.split(':')[0] not in MODULE_SHADOWS_CLASS]
    unexpected_missing = sorted(set(missing) - NOT_INDEXED)
    assert not unexpected_wrong, (
        f'{len(unexpected_wrong)} name(s) differ from cvista.all: {unexpected_wrong[:5]}'
    )
    assert not unexpected_missing, (
        f'{len(unexpected_missing)} name(s) missing from the flat namespace: '
        f'{unexpected_missing[:5]}'
    )


@pytest.mark.parametrize(
    ('name', 'expected'),
    [
        ('VTK_DOUBLE_MAX', 1.0e299),
        ('VTK_DOUBLE_MIN', -1.0e299),
        ('VTK_LONG_MAX', 2**63 - 1),
        ('VTK_LONG_MIN', -(2**63)),
    ],
)
def test_limits_come_from_the_compiled_module(name, expected):
    """The six constants that regressed, spelled out.

    VTK_FLOAT_MAX/MIN are left to the comparison test above: the compiled value
    is the float32 limit rounded through a double and is not worth restating as
    a literal here.
    """
    import cvista

    assert getattr(cvista, name) == expected


def test_helpers_still_reachable():
    """Fixing precedence must not cost the helper names their flat entries."""
    import cvista

    assert cvista.numpy_to_vtk is importlib.import_module(
        'cvista.util.numpy_support'
    ).numpy_to_vtk
    # all.py DOES star-import util.vtkVariant, so the Python function -- not the
    # compiled class of the same name -- is the correct answer here.
    assert cvista.vtkVariantEqual is importlib.import_module(
        'cvista.util.vtkVariant'
    ).vtkVariantEqual
