"""Regression checks for selecting DLLs from the matching Python build."""

import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from repair_windows import _bin_dirs
import cvista_backend


class RepairWindowsTests(unittest.TestCase):
    def test_windows_does_not_advertise_abi3(self):
        with patch.object(cvista_backend.sys, "platform", "win32"):
            with patch.dict(os.environ, {"CVISTA_ABI3": "1"}, clear=True):
                self.assertFalse(cvista_backend._abi3_enabled())

    def test_legacy_does_not_search_other_python_builds(self):
        with (
            tempfile.TemporaryDirectory() as project,
            patch.dict(os.environ, {}, clear=True),
        ):
            old = Path(project, "build-cibw-cp310-win_amd64", "bin")
            current = Path(project, "build-cibw-cp311-win_amd64", "bin")
            old.mkdir(parents=True)
            current.mkdir(parents=True)
            self.assertEqual(
                _bin_dirs(project, "cvista-1-cp311-cp311-win_amd64.whl"), [str(current)]
            )
            current.rmdir()
            with self.assertRaises(FileNotFoundError):
                _bin_dirs(project, "cvista-1-cp311-cp311-win_amd64.whl")

    def test_abi3_honors_build_directory_override(self):
        with tempfile.TemporaryDirectory() as project:
            base = str(Path(project, "custom"))
            current = Path(base + "-abi3", "bin")
            current.mkdir(parents=True)
            with patch.dict(os.environ, {"CVISTA_BUILD_DIR": base}, clear=True):
                self.assertEqual(
                    _bin_dirs(project, "cvista-1-cp312-abi3-win_amd64.whl"),
                    [str(current)],
                )


if __name__ == "__main__":
    unittest.main()
