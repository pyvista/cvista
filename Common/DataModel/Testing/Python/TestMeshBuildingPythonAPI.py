"""Tests for vtkCellArray and vtkPoints Pythonic constructors."""

from vtkmodules.vtkCommonCore import vtkPoints
from vtkmodules.vtkCommonDataModel import vtkCellArray
from vtkmodules.util.numpy_support import numpy_to_vtk
import numpy as np
from vtkmodules.test import Testing


class TestCellArrayConstructor(Testing.vtkTest):
    def test_empty_constructor(self):
        ca = vtkCellArray()
        self.assertEqual(ca.GetNumberOfCells(), 0)

    def test_constructor_from_numpy(self):
        offsets = np.array([0, 3, 6], dtype=np.int64)
        connectivity = np.array([0, 1, 2, 3, 4, 5], dtype=np.int64)
        ca = vtkCellArray(offsets=offsets, connectivity=connectivity)
        self.assertEqual(ca.GetNumberOfCells(), 2)
        np.testing.assert_array_equal(
            np.asarray(ca.GetOffsetsArray()), offsets
        )
        np.testing.assert_array_equal(
            np.asarray(ca.GetConnectivityArray()), connectivity
        )

    def test_constructor_from_lists(self):
        ca = vtkCellArray(offsets=[0, 3, 6], connectivity=[0, 1, 2, 3, 4, 5])
        self.assertEqual(ca.GetNumberOfCells(), 2)

    def test_constructor_from_vtk_arrays(self):
        offsets = numpy_to_vtk(np.array([0, 4], dtype=np.int64))
        connectivity = numpy_to_vtk(np.array([0, 1, 2, 3], dtype=np.int64))
        ca = vtkCellArray(offsets=offsets, connectivity=connectivity)
        self.assertEqual(ca.GetNumberOfCells(), 1)

    def test_partial_kwargs_raises(self):
        with self.assertRaises(ValueError):
            vtkCellArray(offsets=[0, 3])
        with self.assertRaises(ValueError):
            vtkCellArray(connectivity=[0, 1, 2])

    def test_offsets_connectivity_properties(self):
        offsets = np.array([0, 3, 5], dtype=np.int64)
        connectivity = np.array([0, 1, 2, 3, 4], dtype=np.int64)
        ca = vtkCellArray(offsets=offsets, connectivity=connectivity)
        # offsets_array and connectivity_array are auto-generated properties
        self.assertIsNotNone(ca.offsets_array)
        self.assertIsNotNone(ca.connectivity_array)

    def test_repr(self):
        ca = vtkCellArray()
        self.assertEqual(repr(ca), "vtkCellArray(0 cells)")
        offsets = np.array([0, 3, 6, 10], dtype=np.int64)
        connectivity = np.arange(10, dtype=np.int64)
        ca = vtkCellArray(offsets=offsets, connectivity=connectivity)
        self.assertEqual(repr(ca), "vtkCellArray(3 cells)")


class TestPointsConstructor(Testing.vtkTest):
    def test_empty_constructor(self):
        pts = vtkPoints()
        self.assertEqual(pts.GetNumberOfPoints(), 0)

    def test_constructor_from_numpy(self):
        data = np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])
        pts = vtkPoints(data=data)
        self.assertEqual(pts.GetNumberOfPoints(), 3)
        np.testing.assert_array_almost_equal(pts.GetPoint(0), [0, 0, 0])
        np.testing.assert_array_almost_equal(pts.GetPoint(1), [1, 0, 0])
        np.testing.assert_array_almost_equal(pts.GetPoint(2), [0, 1, 0])

    def test_constructor_from_list_of_lists(self):
        pts = vtkPoints(data=[[0, 0, 0], [1, 0, 0]])
        self.assertEqual(pts.GetNumberOfPoints(), 2)
        np.testing.assert_array_almost_equal(pts.GetPoint(0), [0, 0, 0])
        np.testing.assert_array_almost_equal(pts.GetPoint(1), [1, 0, 0])

    def test_constructor_from_vtk_array(self):
        data = np.array([[0.0, 0.0, 0.0], [1.0, 2.0, 3.0]])
        vtk_arr = numpy_to_vtk(data)
        pts = vtkPoints(data=vtk_arr)
        self.assertEqual(pts.GetNumberOfPoints(), 2)
        np.testing.assert_array_almost_equal(pts.GetPoint(1), [1, 2, 3])

    def test_repr(self):
        pts = vtkPoints()
        self.assertEqual(repr(pts), "vtkPoints(0 points)")
        data = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
        pts = vtkPoints(data=data)
        self.assertEqual(repr(pts), "vtkPoints(3 points)")

    def test_getitem(self):
        data = np.array(
            [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]]
        )
        pts = vtkPoints(data=data)
        self.assertEqual(pts.GetNumberOfPoints(), 3)
        np.testing.assert_array_almost_equal(pts.GetPoint(0), [1, 2, 3])
        np.testing.assert_array_almost_equal(pts.GetPoint(2), [7, 8, 9])


if __name__ == "__main__":
    Testing.main([(TestCellArrayConstructor, "test"),
                   (TestPointsConstructor, "test")])
