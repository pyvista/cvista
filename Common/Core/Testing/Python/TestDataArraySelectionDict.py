"""Tests for the dictionary interface on vtkDataArraySelection."""

import sys
from vtkmodules.vtkCommonCore import vtkDataArraySelection
from vtkmodules.test import Testing


class TestDataArraySelectionDict(Testing.vtkTest):
    def setUp(self):
        self.sel = vtkDataArraySelection()
        self.sel.AddArray("Pressure")
        self.sel.AddArray("Velocity")
        self.sel.AddArray("Temperature")
        self.sel.EnableAllArrays()

    def test_getitem(self):
        """__getitem__ returns bool for existing arrays."""
        self.sel.EnableArray("Pressure")
        self.assertIs(self.sel["Pressure"], True)
        self.sel.DisableArray("Velocity")
        self.assertIs(self.sel["Velocity"], False)

    def test_getitem_keyerror(self):
        """__getitem__ raises KeyError for missing arrays."""
        with self.assertRaises(KeyError):
            _ = self.sel["NonExistent"]

    def test_setitem_enable_disable(self):
        """__setitem__ enables/disables arrays."""
        self.sel["Pressure"] = False
        self.assertEqual(self.sel.ArrayIsEnabled("Pressure"), 0)
        self.sel["Pressure"] = True
        self.assertEqual(self.sel.ArrayIsEnabled("Pressure"), 1)

    def test_setitem_creates_new(self):
        """__setitem__ creates new entries if they don't exist."""
        self.sel["NewArray"] = True
        self.assertTrue(self.sel.ArrayExists("NewArray"))
        self.assertEqual(self.sel.ArrayIsEnabled("NewArray"), 1)

    def test_delitem(self):
        """__delitem__ removes arrays."""
        del self.sel["Velocity"]
        self.assertFalse(self.sel.ArrayExists("Velocity"))
        self.assertEqual(self.sel.GetNumberOfArrays(), 2)

    def test_delitem_keyerror(self):
        """__delitem__ raises KeyError for missing arrays."""
        with self.assertRaises(KeyError):
            del self.sel["NonExistent"]

    def test_contains(self):
        """__contains__ checks array existence."""
        self.assertIn("Pressure", self.sel)
        self.assertNotIn("NonExistent", self.sel)

    def test_len(self):
        """__len__ returns number of arrays."""
        self.assertEqual(len(self.sel), 3)
        self.sel.AddArray("Extra")
        self.assertEqual(len(self.sel), 4)

    def test_iter(self):
        """__iter__ iterates over array names."""
        names = list(self.sel)
        self.assertEqual(names, ["Pressure", "Velocity", "Temperature"])

    def test_keys(self):
        """keys() returns list of array names."""
        self.assertEqual(self.sel.keys(), ["Pressure", "Velocity", "Temperature"])

    def test_values(self):
        """values() returns list of booleans."""
        self.sel.DisableArray("Velocity")
        vals = self.sel.values()
        self.assertEqual(vals, [True, False, True])
        self.assertIsInstance(vals[0], bool)

    def test_items(self):
        """items() returns list of (name, bool) pairs."""
        self.sel.DisableArray("Temperature")
        items = self.sel.items()
        self.assertEqual(
            items,
            [("Pressure", True), ("Velocity", True), ("Temperature", False)],
        )

    def test_dict_conversion(self):
        """Can convert to a plain dict via dict(sel.items())."""
        self.sel.DisableArray("Velocity")
        d = dict(self.sel.items())
        self.assertEqual(d, {"Pressure": True, "Velocity": False, "Temperature": True})

    def test_repr(self):
        """__repr__ shows count summary."""
        self.sel.DisableArray("Velocity")
        r = repr(self.sel)
        self.assertEqual(r, "vtkDataArraySelection(3 arrays, 2 enabled)")

    def test_enable_disable_all_still_work(self):
        """C++ methods EnableAllArrays/DisableAllArrays still work."""
        self.sel.DisableAllArrays()
        self.assertEqual(self.sel.values(), [False, False, False])
        self.sel.EnableAllArrays()
        self.assertEqual(self.sel.values(), [True, True, True])

    def test_empty_selection(self):
        """Empty selection works correctly."""
        empty = vtkDataArraySelection()
        self.assertEqual(len(empty), 0)
        self.assertEqual(list(empty), [])
        self.assertEqual(empty.keys(), [])
        self.assertEqual(empty.values(), [])
        self.assertEqual(empty.items(), [])
        self.assertEqual(repr(empty), "vtkDataArraySelection(0 arrays, 0 enabled)")


if __name__ == "__main__":
    Testing.main([(TestDataArraySelectionDict, "test")])
