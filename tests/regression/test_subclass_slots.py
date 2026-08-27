#!/usr/bin/env python
"""Regression: a wrapper's instance memory must be zeroed when it is allocated.

PyVTKObject_FromPointer() allocated wrappers with PyObject_GC_New, which
reserves tp_basicsize bytes and initializes only the object header.  A Python
subclass that declares __slots__ has a larger tp_basicsize than PyVTKObject,
so its slot storage came back holding whatever the recycled allocator block
last contained.  CPython treats a slot as an owned reference: the first
assignment Py_XDECREFs the old value, and clear_slots() Py_DECREFs it again
when the instance is deallocated.  Both dereference the garbage, so any
subclass with __slots__ crashes the interpreter as soon as it lands on a block
that is not already zeroed.

Allocating with PyType_GenericAlloc zeroes the whole instance, so an unset
slot reads as NULL and behaves like any other unset slot.
"""

import ctypes
import gc

import pytest

from cvista.vtkCommonCore import vtkObject

# Byte written into the recycled blocks.  Any non-zero pattern works; 0x41 is
# picked because it is obvious in a hex dump if this test ever does report a
# failure.
FILL = 0x41

# CPython's GC pre-header, which sits in front of every tracked object and is
# part of the block the allocator hands out.
GC_PRESIZE = 16

# sizeof(PyBytesObject) minus its one-byte ob_sval[], plus the trailing NUL,
# i.e. the constant overhead of a bytes object's allocation.
BYTES_OVERHEAD = 33


class SlottedObject(vtkObject):
    __slots__ = ('payload',)


def _dirty_the_size_class(block_size, copies=2000):
    """Leave freed blocks in the size class of ``block_size`` full of FILL."""
    # A bytes object of length n allocates BYTES_OVERHEAD + n bytes.  pymalloc
    # rounds that up to a 16-byte size class, so sweep a range of lengths to be
    # sure one of them lands in the same class as the wrapper regardless of how
    # the interpreter rounds.
    junk = []
    for length in range(block_size - BYTES_OVERHEAD - 24, block_size - BYTES_OVERHEAD + 8):
        if length > 0:
            junk.extend(bytes([FILL]) * length for _ in range(copies))
    del junk
    gc.collect()


def test_unset_slot_is_empty():
    obj = SlottedObject()
    with pytest.raises(AttributeError):
        obj.payload
    obj.payload = 'set'
    assert obj.payload == 'set'
    del obj.payload
    with pytest.raises(AttributeError):
        obj.payload


def test_slot_storage_is_zeroed_on_a_dirty_heap():
    base_size = vtkObject.__basicsize__
    slot_size = SlottedObject.__basicsize__
    assert slot_size > base_size, '__slots__ did not extend the instance'

    _dirty_the_size_class(GC_PRESIZE + slot_size)

    # Under the bug this reads back FILL bytes; the assignment below then
    # decrefs them and takes the process with it.
    obj = SlottedObject()
    tail = ctypes.string_at(id(obj) + base_size, slot_size - base_size)
    assert tail == bytes(len(tail)), f'slot storage was not zeroed: {tail.hex()}'

    obj.payload = 'set'
    assert obj.payload == 'set'
    del obj


def test_many_slotted_instances_survive_a_dirty_heap():
    _dirty_the_size_class(GC_PRESIZE + SlottedObject.__basicsize__)
    for i in range(1000):
        obj = SlottedObject()
        obj.payload = i
        assert obj.payload == i
        del obj
