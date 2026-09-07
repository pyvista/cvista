#!/usr/bin/env python
"""Regression: creating a wrapper ghost must not visit every other live ghost.

When a Python wrapper with a customized class or a non-empty __dict__ is
destroyed while its C++ object is still alive, vtkPythonUtil::RemoveObjectFromMap
keeps a ghost of it so that the class and dict can be restored if the C++
object comes back to Python.  Before inserting the new ghost it walked the
whole ghost map to erase the ghosts whose C++ object had died, an O(number of
live ghosts) sweep on every ghost creation.  The numpy interface overrides give
nearly every wrapper a non-empty __dict__, so nearly every discarded wrapper
becomes a ghost and the sweep ran on almost every wrapper deletion, making the
cost of discarding a wrapper grow with the number of wrapped VTK objects alive
in the session.

Ghosts of vtkObjects are evicted by a DeleteEvent observer the moment the C++
object dies.  Only the ghosts of vtkObjectBase subclasses that are not
vtkObjects (vtkCommand, vtkInformationKey, vtkLogger) need the sweep, so they
are kept in a separate map and only that map is swept.
"""

import gc
import sys
import timeit
import weakref

from cvista.vtkCommonCore import vtkCallbackCommand, vtkObject, vtkVariantArray


def _collect():
    """Force a collection where deallocation is not immediate."""
    if hasattr(sys, "_is_gil_enabled") and not sys._is_gil_enabled():
        gc.collect()


def test_ghost_creation_cost_is_independent_of_ghost_count():
    a = vtkVariantArray()
    a.InsertNextValue(vtkObject())

    def roundtrip():
        # Resurrects the ghost, then ghosts the wrapper again when the
        # temporary goes away.
        a.GetValue(0).ToVTKObject().customattr = "hello"

    def measure():
        gc.disable()
        try:
            return min(timeit.repeat(roundtrip, number=500, repeat=5))
        finally:
            gc.enable()

    roundtrip()
    baseline = measure()

    # Keep many ghosts alive: each vtkObject stays held by C++ after its
    # wrapper with a custom attribute has gone away.
    holders = []
    for _ in range(20000):
        o = vtkObject()
        o.customattr = "hello"
        h = vtkVariantArray()
        h.InsertNextValue(o)
        holders.append(h)
    del o, h
    _collect()

    loaded = measure()
    # A sweep over all ghosts makes this ratio grow linearly with the number
    # of live ghosts; the bound is generous so that slow or busy machines do
    # not trip it.
    assert loaded < 20 * baseline


def test_non_vtkobject_ghost_round_trip():
    c = vtkCallbackCommand()
    c.customattr = "hello"
    a = vtkVariantArray()
    a.InsertNextValue(c)
    original_id = id(c)
    del c
    _collect()

    _filler = vtkObject()
    c2 = a.GetValue(0).ToVTKObject()
    assert isinstance(c2, vtkCallbackCommand)
    assert c2.customattr == "hello"
    assert id(c2) != original_id


def test_non_vtkobject_ghost_evicted_by_sweep():
    class Payload:
        pass

    c = vtkCallbackCommand()
    c.payload = Payload()
    payload_ref = weakref.ref(c.payload)
    a = vtkVariantArray()
    a.InsertNextValue(c)
    del c
    _collect()
    # The ghost keeps the __dict__, and with it the payload, alive.
    assert payload_ref() is not None

    # vtkCommand has no DeleteEvent, so destroying the C++ object leaves a
    # stale ghost behind until another ghost is created.
    del a
    _collect()
    o = vtkObject()
    o.customattr = "hello"
    b = vtkVariantArray()
    b.InsertNextValue(o)
    del o
    _collect()
    assert payload_ref() is None
