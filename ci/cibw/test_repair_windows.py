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
        # CPython <=3.12 on Windows omits SOABI; newer versions provide it.
        for minor, soabi in ((10, None), (11, None), (12, None),
                             (13, "cp313-win_amd64"), (14, "cp314-win_amd64")):
            with (
                self.subTest(minor=minor),
                tempfile.TemporaryDirectory() as project,
                patch.dict(os.environ, {}, clear=True),
                patch.object(cvista_backend.sys, "platform", "win32"),
                patch.object(cvista_backend.sys, "version_info", (3, minor)),
                patch("sysconfig.get_config_var", return_value=soabi),
            ):
                Path(project, "build-cibw-py39", "bin").mkdir(parents=True)
                suffix = soabi or f"py3{minor}"
                current = Path(project, f"build-cibw-{suffix}", "bin")
                current.mkdir(parents=True)
                wheel = f"cvista-1-cp3{minor}-cp3{minor}-win_amd64.whl"
                self.assertEqual(_bin_dirs(project, wheel), [str(current)])
                current.rmdir()
                with self.assertRaises(FileNotFoundError):
                    _bin_dirs(project, wheel)

    def test_repair_rejects_wrong_interpreter(self):
        with patch.object(cvista_backend.sys, "version_info", (3, 12)):
            with self.assertRaisesRegex(ValueError, "build interpreter"):
                _bin_dirs("unused", "cvista-1-cp311-cp311-win_amd64.whl")

    def test_build_directory_override(self):
        with tempfile.TemporaryDirectory() as project:
            base = str(Path(project, "custom"))
            current = Path(base + "-py312", "bin")
            current.mkdir(parents=True)
            with (
                patch.dict(os.environ, {"CVISTA_BUILD_DIR": base}, clear=True),
                patch.object(cvista_backend.sys, "platform", "win32"),
                patch.object(cvista_backend.sys, "version_info", (3, 12)),
                patch("sysconfig.get_config_var", return_value=None),
            ):
                self.assertEqual(
                    _bin_dirs(project, "cvista-1-cp312-cp312-win_amd64.whl"),
                    [str(current)],
                )


if __name__ == "__main__":
    unittest.main()
