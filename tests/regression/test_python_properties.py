#!/usr/bin/env python
"""Regression: the default build exposes pythonic snake_case properties.

VTK 9.4+ auto-generates pythonic snake_case @property descriptors alongside the
camelCase Get/Set methods for every wrapped class (e.g. GetVisibility() /
SetVisibility() also become obj.visibility). fvtk exposes a build option,
FVTK_DISABLE_PYTHON_PROPERTIES, to suppress those descriptors; it defaults OFF,
which must leave the generated wrappers byte-identical to stock.

This test pins the default: it asserts a known snake_case property is present on
a wrapped class. It is import-only (no rendering). If the option were
accidentally enabled in a default build, or the descriptor generation regressed,
the snake_case attribute would disappear and this test would fail.
"""

from fvtk.vtkRenderingCore import vtkActor


def test_snake_case_property_present_by_default():
    """vtkActor exposes the snake_case `visibility` property descriptor.

    `Visibility` is a vtkSetGet macro pair on vtkProp (vtkActor's base), so the
    9.4+ wrapper turns it into a `visibility` @property on the type. Checking the
    type (not the instance) confirms it is a real descriptor, not a stray ivar.
    """
    assert hasattr(
        vtkActor, "visibility"
    ), "snake_case property descriptors are missing from the default build"

    # It must round-trip through the descriptor on an instance.
    a = vtkActor()
    a.visibility = False
    assert a.visibility == 0
    assert a.GetVisibility() == 0
    a.visibility = True
    assert a.visibility == 1
    assert a.GetVisibility() == 1
