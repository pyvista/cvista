"""Checks for the `pyvista build cvista` hooks and the manifest they serve."""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib
import unittest
from unittest.mock import patch
import zipfile

REPO = Path(__file__).resolve().parents[2]
HOOK = REPO / "ci" / "pyvista_build_hooks.py"
sys.path.insert(0, str(REPO / "ci"))

import pyvista_build_hooks as hooks  # noqa: E402


def _write_cache(build_dir: str, value: str) -> None:
    Path(build_dir, "CMakeCache.txt").write_text(
        f"// cache\nCMAKE_BUILD_TYPE:STRING=Release\nCVISTA_ABI3:BOOL={value}\n"
    )


def _fake_wheel(dist: Path, name: str) -> Path:
    wheel = dist / name
    with zipfile.ZipFile(wheel, "w") as z:
        z.writestr("cvista-9.7.0.5.dist-info/WHEEL", "Wheel-Version: 1.0\nTag: cp312-cp312-linux_x86_64\n")
        z.writestr("cvista-9.7.0.5.dist-info/RECORD", "cvista-9.7.0.5.dist-info/WHEEL,sha256=x,1\n")
    return wheel


class VersionTests(unittest.TestCase):
    @unittest.skipUnless(
        importlib.util.find_spec("setuptools_scm") or hooks._tag_suffix() is not None,
        "needs setuptools_scm or a release tag on HEAD",
    )
    def test_stdout_is_one_line(self):
        out = subprocess.run(
            [sys.executable, str(HOOK), "version"], cwd=REPO, capture_output=True, text=True, check=True
        ).stdout
        self.assertEqual(len(out.splitlines()), 1, out)
        self.assertRegex(out.strip(), r"^(\d+(\.\d+)*(\.dev\d+)?|dev0|0\.dev0|)$")

    def test_exact_tag(self):
        base = hooks.cvista_backend.VTK_BASE_VERSION
        completed = subprocess.CompletedProcess([], 0, stdout=f"{base}.7\n", stderr="")
        with patch.object(hooks.subprocess, "run", return_value=completed):
            self.assertEqual(hooks._tag_suffix(), "7")
        completed.stdout = f"{base}\n"
        with patch.object(hooks.subprocess, "run", return_value=completed):
            self.assertEqual(hooks._tag_suffix(), "")
        completed.stdout = "v1\n"
        with patch.object(hooks.subprocess, "run", return_value=completed):
            self.assertIsNone(hooks._tag_suffix())
        err = subprocess.CalledProcessError(128, ["git"])
        with patch.object(hooks.subprocess, "run", side_effect=err):
            self.assertIsNone(hooks._tag_suffix())


class CacheTests(unittest.TestCase):
    def test_abi3_configured(self):
        with tempfile.TemporaryDirectory() as build:
            _write_cache(build, "ON")
            self.assertTrue(hooks._abi3_configured(build))
            _write_cache(build, "OFF")
            self.assertFalse(hooks._abi3_configured(build))
            os.remove(os.path.join(build, "CMakeCache.txt"))
            with self.assertRaises(SystemExit):
                hooks._abi3_configured(build)

    def test_check(self):
        with tempfile.TemporaryDirectory() as build:
            _write_cache(build, "OFF")
            with patch.object(hooks.sys, "version_info", (3, 11)):
                hooks.check(build)
            _write_cache(build, "ON")
            with patch.object(hooks.sys, "version_info", (3, 11)), self.assertRaises(SystemExit):
                hooks.check(build)
            with (
                patch.object(hooks.sys, "version_info", (3, 12)),
                patch.object(hooks.sys, "platform", "win32"),
                self.assertRaises(SystemExit),
            ):
                hooks.check(build)
            with (
                patch.object(hooks.sys, "version_info", (3, 12)),
                patch.object(hooks.sys, "platform", "linux"),
            ):
                hooks.check(build)


class RetagTests(unittest.TestCase):
    def test_retag(self):
        with tempfile.TemporaryDirectory() as build:
            dist = Path(build, "dist")
            dist.mkdir()
            _write_cache(build, "ON")
            with self.assertRaises(SystemExit):
                hooks.retag(build)
            wheel = _fake_wheel(dist, "cvista-9.7.0.5-cp312-cp312-linux_x86_64.whl")
            _write_cache(build, "OFF")
            hooks.retag(build)
            self.assertTrue(wheel.exists())
            _write_cache(build, "ON")
            hooks.retag(build)
            self.assertEqual(
                sorted(p.name for p in dist.iterdir()), ["cvista-9.7.0.5-cp312-abi3-linux_x86_64.whl"]
            )
            with zipfile.ZipFile(dist / "cvista-9.7.0.5-cp312-abi3-linux_x86_64.whl") as z:
                self.assertIn("Tag: cp312-abi3-linux_x86_64", z.read("cvista-9.7.0.5.dist-info/WHEEL").decode())


class ManifestTests(unittest.TestCase):
    def test_manifest_paths_and_tag(self):
        with open(REPO / "pyvista-build.toml", "rb") as f:
            manifest = tomllib.load(f)
        for path in manifest["cmake"]["init_cache"].values():
            self.assertTrue((REPO / path).is_file(), path)
        for commands in manifest["hooks"].values():
            for command in commands:
                script = command.split()[1]
                self.assertTrue((REPO / script).is_file(), script)
        self.assertEqual(manifest["backend"]["version_command"].split()[1], "ci/pyvista_build_hooks.py")
        tags = subprocess.run(
            ["git", "tag", "--list"], cwd=REPO, capture_output=True, text=True, check=True
        ).stdout.split()
        self.assertIn(manifest["backend"]["default_ref"], tags)
        self.assertTrue(re.fullmatch(r"\d+(\.\d+)+", manifest["backend"]["default_ref"]))


if __name__ == "__main__":
    unittest.main()
