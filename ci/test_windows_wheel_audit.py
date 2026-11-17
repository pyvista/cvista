"""Checks for the Python DLL import audit of Windows wheels."""

from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

import test_downloaded_windows_wheels as audit  # noqa: E402


class _Import:
    def __init__(self, dll: str) -> None:
        self.dll = dll.encode()


class _FakePE:
    """Stand-in for pefile.PE: the archive member's bytes name the DLLs it imports."""

    def __init__(self, data: bytes) -> None:
        self.DIRECTORY_ENTRY_IMPORT = [_Import(d) for d in data.decode().split() if d]

    def close(self) -> None:
        pass


def _wheel(directory: str, name: str, members: dict[str, str]) -> str:
    path = Path(directory, name)
    with zipfile.ZipFile(path, "w") as z:
        for member, imports in members.items():
            z.writestr(member, imports)
    return str(path)


class AuditTests(unittest.TestCase):
    def setUp(self):
        fake = types.SimpleNamespace(PE=_FakePE)
        patcher = patch.dict(sys.modules, {"pefile": fake})
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_accepts_own_runtime(self):
        with tempfile.TemporaryDirectory() as d:
            audit.audit_python_imports(_wheel(d, "cvista-1-cp312-abi3-win_amd64.whl", {
                "cvista/a.pyd": "python3.dll kernel32.dll",
                "cvista/b.dll": "vcruntime140.dll",
                "cvista/notes.txt": "python311.dll",
            }))
            audit.audit_python_imports(_wheel(d, "cvista-1-cp311-cp311-win_amd64.whl", {
                "cvista/a.pyd": "python311.dll",
                "cvista/b.pyd": "python3.dll",
            }))

    def test_rejects_other_runtime(self):
        with tempfile.TemporaryDirectory() as d:
            wheel = _wheel(d, "cvista-1-cp312-abi3-win_amd64.whl", {
                "cvista/a.pyd": "python3.dll",
                "cvista/b.pyd": "python311.dll",
            })
            with self.assertRaisesRegex(RuntimeError, r"b\.pyd: python311\.dll"):
                audit.audit_python_imports(wheel)
            wheel = _wheel(d, "cvista-1-cp311-cp311-win_amd64.whl", {
                "cvista/a.pyd": "python312_d.dll",
            })
            with self.assertRaisesRegex(RuntimeError, "python312_d.dll"):
                audit.audit_python_imports(wheel)

    def test_rejects_unknown_filename(self):
        with self.assertRaises(ValueError):
            audit.audit_python_imports("cvista-1-py3-none-any.whl")

    def test_audit_entrypoint(self):
        with tempfile.TemporaryDirectory() as d:
            good = _wheel(d, "cvista-1-cp312-abi3-win_amd64.whl", {"cvista/a.pyd": "python3.dll"})
            bad = _wheel(d, "cvista-1-cp313-cp313-win_amd64.whl", {"cvista/a.pyd": "python312.dll"})
            with patch.object(sys, "argv", ["audit.py", "audit", good]):
                audit.main()
            with patch.object(sys, "argv", ["audit.py", "audit", good, bad]):
                with self.assertRaises(RuntimeError):
                    audit.main()

    def test_accepts_plain_vtk_names_and_private_runtime(self):
        msvcp = "msvcp140-06f78564ee5e6dd552da1760ee946703.dll"
        vcomp = "vcomp140-f96f3a14d88d8846f31f3ab38a490304.dll"
        with tempfile.TemporaryDirectory() as d:
            audit.audit_dll_names(_wheel(d, "cvista-1-cp311-cp311-win_amd64.whl", {
                "cvista/vtkCommonCore.pyd": f"vtkCommon-cvista.dll {msvcp} vcruntime140.dll",
                "cvista.libs/vtkFilters-cvista.dll": f"vtkCommon-cvista.dll {msvcp} {vcomp}",
                f"cvista.libs/{msvcp}": "vcruntime140.dll vcruntime140_1.dll",
                f"cvista.libs/{vcomp}": "kernel32.dll",
            }))

    def test_rejects_unmangled_runtime(self):
        with tempfile.TemporaryDirectory() as d:
            wheel = _wheel(d, "cvista-1-cp312-abi3-win_amd64.whl", {
                "cvista.libs/vtkCommon-cvista.dll": "msvcp140.dll",
                "cvista.libs/msvcp140.dll": "vcruntime140.dll",
                "cvista.libs/vtkFilters-cvista.dll": "vcomp140.dll",
            })
            with self.assertRaises(RuntimeError) as caught:
                audit.audit_dll_names(wheel)
            message = str(caught.exception)
            self.assertIn("vtkCommon-cvista.dll: imports unmangled msvcp140.dll", message)
            self.assertIn("cvista.libs/msvcp140.dll: bundled MSVC runtime is not mangled", message)
            self.assertIn("vtkFilters-cvista.dll: imports unmangled vcomp140.dll", message)

    def test_rejects_mangled_vtk_names(self):
        mangled = "vtkCommon-cvista-79504741e463465be3e74e574191dcb7.dll"
        with tempfile.TemporaryDirectory() as d:
            wheel = _wheel(d, "cvista-1-cp312-abi3-win_amd64.whl", {
                "cvista/vtkCommonCore.pyd": mangled,
                f"cvista.libs/{mangled}": "kernel32.dll",
            })
            with self.assertRaises(RuntimeError) as caught:
                audit.audit_dll_names(wheel)
            message = str(caught.exception)
            self.assertIn(f"vtkCommonCore.pyd: imports mangled {mangled.lower()}", message)
            self.assertIn(f"cvista.libs/{mangled}: VTK DLL name is mangled", message)


if __name__ == "__main__":
    unittest.main()
