#!/usr/bin/env python
"""Regression: every numpy dtype PyVista supports behaves like stock VTK.

Two defects, unrelated to each other, both reported in pyvista/cvista#255.

**Concrete classes.** ``vtkShortArray``, ``vtkTypeInt16Array``,
``vtkTypeUInt64Array`` and the two unsigned-long arrays were in
``_nowrap_classes.cmake``, so they compiled without a Python wrapper and
``numpy_to_vtk`` handed back a bare ``vtkDataArray`` for int16 and uint64. That
breaks any downstream ``isinstance`` or ``type(...) is ...`` check the stock
wheel satisfies, and ``cvista.vtkTypeInt16Array()`` raised "this class cannot be
instantiated".

**DeepCopy speed.** ``vtkDataArray::DeepCopy`` dispatched through
``vtkArrayDispatch::Dispatch2``, which only reaches its memcpy specialization for
value types in ``vtkArrayDispatch::Arrays``. With the list trimmed to four types
(``CVISTA_DISPATCH_MINIMAL``), the other six fell through to the virtual
per-tuple path at a flat ~2.5 ns/element: 7x to 75x slower than stock, and it
reached ``PolyData.copy(deep=True)`` through the int32 connectivity array.
``vtkDataArray_DeepCopy.cxx`` now takes the byte copy directly for same-type AOS
arrays, without templates, so the dispatch list can stay trimmed.
"""

from __future__ import annotations

import time

import numpy as np
import pytest

from cvista import numpy_to_vtk
from cvista import vtk_to_numpy

DTYPES = [
    np.int8,
    np.uint8,
    np.int16,
    np.uint16,
    np.int32,
    np.uint32,
    np.int64,
    np.uint64,
    np.float32,
    np.float64,
]

# numpy dtype -> the concrete class stock VTK's numpy_to_vtk returns.
EXPECTED_CLASS = {
    np.int8: 'vtkTypeInt8Array',
    np.uint8: 'vtkTypeUInt8Array',
    np.int16: 'vtkTypeInt16Array',
    np.uint16: 'vtkTypeUInt16Array',
    np.int32: 'vtkTypeInt32Array',
    np.uint32: 'vtkTypeUInt32Array',
    np.int64: 'vtkTypeInt64Array',
    np.uint64: 'vtkTypeUInt64Array',
    np.float32: 'vtkTypeFloat32Array',
    np.float64: 'vtkTypeFloat64Array',
}


@pytest.mark.parametrize('dtype', DTYPES, ids=lambda d: np.dtype(d).name)
def test_numpy_to_vtk_returns_the_concrete_class(dtype):
    """A bare vtkDataArray here means the wrapper was dropped from the build."""
    array = numpy_to_vtk(np.arange(16).astype(dtype), deep=True)

    assert type(array).__name__ == EXPECTED_CLASS[dtype]


@pytest.mark.parametrize('dtype', DTYPES, ids=lambda d: np.dtype(d).name)
def test_deep_copy_round_trips(dtype):
    """DeepCopy must reproduce the values exactly, whichever path it takes."""
    values = (np.arange(1000) % 100).astype(dtype)
    array = numpy_to_vtk(values, deep=True)

    copied = array.NewInstance()
    copied.DeepCopy(array)

    assert copied.GetNumberOfTuples() == array.GetNumberOfTuples()
    assert copied.GetNumberOfComponents() == array.GetNumberOfComponents()
    np.testing.assert_array_equal(vtk_to_numpy(copied), values)


@pytest.mark.parametrize('dtype', DTYPES, ids=lambda d: np.dtype(d).name)
def test_deep_copy_multi_component_round_trips(dtype):
    """The byte copy multiplies by component count; a wrong stride shows here."""
    values = (np.arange(999) % 100).astype(dtype).reshape(333, 3)
    array = numpy_to_vtk(values, deep=True)

    copied = array.NewInstance()
    copied.DeepCopy(array)

    assert copied.GetNumberOfComponents() == 3
    np.testing.assert_array_equal(vtk_to_numpy(copied), values)


def _deep_copy_ms_per_gb(dtype, n=2_000_000):
    values = (np.arange(n) % 100).astype(dtype)
    array = numpy_to_vtk(values, deep=True)
    best = min(_time_one(array) for _ in range(5))
    return best * 1e3 / (n * np.dtype(dtype).itemsize / 1e9)


def _time_one(array):
    start = time.perf_counter()
    array.NewInstance().DeepCopy(array)
    return time.perf_counter() - start


@pytest.mark.parametrize('dtype', [np.int8, np.int16, np.uint16, np.int32, np.uint32, np.uint64],
                         ids=lambda d: np.dtype(d).name)
def test_deep_copy_is_not_on_the_per_element_fallback(dtype):
    """Every value type must copy at roughly memory bandwidth, not per element.

    Compared against float64 in the same process rather than an absolute
    threshold, so this does not turn into a benchmark that fails on a loaded
    runner. Normalised per byte, the six affected types were 8x to 75x worse than
    float64 before the fix and land within ~2x of it after, so 5x separates them
    with room to spare.
    """
    reference = _deep_copy_ms_per_gb(np.float64)
    measured = _deep_copy_ms_per_gb(dtype)

    assert measured < reference * 5, (
        f'{np.dtype(dtype).name} deep copy is {measured:.0f} ms/GB against '
        f'{reference:.0f} ms/GB for float64. That ratio means it is on the virtual '
        f'per-tuple fallback rather than the byte copy in vtkDataArray_DeepCopy.cxx.'
    )


@pytest.mark.parametrize('dtype', DTYPES, ids=lambda d: np.dtype(d).name)
def test_copy_component_round_trips(dtype):
    """CopyComponent is strided, so a wrong stride or width shows up here."""
    values = (np.arange(300) % 100).astype(dtype).reshape(100, 3)
    array = numpy_to_vtk(values, deep=True)

    single = array.NewInstance()
    single.SetNumberOfComponents(1)
    single.SetNumberOfTuples(100)
    single.CopyComponent(0, array, 1)

    np.testing.assert_array_equal(vtk_to_numpy(single), values[:, 1])


@pytest.mark.parametrize('dtype', DTYPES, ids=lambda d: np.dtype(d).name)
def test_insert_and_get_tuples_round_trip(dtype):
    """The byte paths under InsertTuples/GetTuples must preserve order and values."""
    values = (np.arange(200) % 100).astype(dtype)
    array = numpy_to_vtk(values, deep=True)

    inserted = array.NewInstance()
    inserted.SetNumberOfComponents(1)
    inserted.InsertTuples(0, 200, 0, array)
    np.testing.assert_array_equal(vtk_to_numpy(inserted), values)

    got = array.NewInstance()
    got.SetNumberOfComponents(1)
    got.SetNumberOfTuples(50)
    array.GetTuples(10, 59, got)
    np.testing.assert_array_equal(vtk_to_numpy(got), values[10:60])


@pytest.mark.parametrize(
    ('src_dtype', 'dst_dtype'),
    [(np.float32, np.float64), (np.int32, np.float64), (np.uint8, np.float32)],
    ids=['f32-f64', 'i32-f64', 'u8-f32'],
)
def test_cross_type_deep_copy_still_converts(src_dtype, dst_dtype):
    """Cross-type conversion keeps the dispatcher; the byte path must not steal it.

    A byte copy here would reinterpret the bits instead of converting, so this
    fails loudly if the same-type guard is ever loosened.
    """
    values = (np.arange(500) % 100).astype(src_dtype)
    src = numpy_to_vtk(values, deep=True)
    dst = numpy_to_vtk(np.zeros(500, dtype=dst_dtype), deep=True)

    dst.DeepCopy(src)

    np.testing.assert_array_equal(vtk_to_numpy(dst), values.astype(dst_dtype))
