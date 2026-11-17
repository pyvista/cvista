"""Test the three abi3 Windows wheels in a directory, or `audit <wheel>...` their DLL imports."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import zipfile


def run(*args):
    subprocess.run([sys.executable, *map(str, args)], check=True, timeout=120)


def audit_python_imports(wheel: str) -> None:
    """Reject repaired wheels that import another interpreter's Python DLL."""
    import pefile

    match = re.search(r"-(cp\d+)-(abi3|cp\d+)-win", Path(wheel).name)
    if match is None:
        raise ValueError(f"Unsupported Windows wheel filename: {wheel}")
    python_tag, abi_tag = match.groups()
    allowed = "python3.dll" if abi_tag == "abi3" else f"python{python_tag[2:]}.dll"
    wrong = []
    with zipfile.ZipFile(wheel) as archive:
        for name in archive.namelist():
            if not name.lower().endswith((".pyd", ".dll")):
                continue
            pe = pefile.PE(data=archive.read(name))
            for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
                dependency = entry.dll.decode().lower()
                if re.fullmatch(r"python\d+(?:_d)?\.dll", dependency):
                    if dependency not in (allowed, "python3.dll"):
                        wrong.append(f"{name}: {dependency}")
            pe.close()
    if wrong:
        raise RuntimeError("Wheel imports the wrong Python runtime:\n" + "\n".join(wrong))


# VTK DLLs keep their build names so the cvista-sdk import libraries resolve (#299).
VTK_DLL = re.compile(r"vtk.*-cvista(?P<hash>-[0-9a-f]{32})?\.dll")
# The MSVC C++ and OpenMP runtimes must be private copies under delvewheel's mangled
# names, or a process that already loaded an older msvcp140.dll binds cvista to it.
# vcruntime140 and vcruntime140_1 come with Python and are never bundled.
MSVC_RUNTIME = re.compile(r"(msvcp\d+(?:_\w+)?|vcomp\d+|concrt\d+)(?P<hash>-[0-9a-f]{32})?\.dll")


def audit_dll_names(wheel: str) -> None:
    """Reject wheels whose VTK DLLs are mangled or whose MSVC runtime is not."""
    import pefile

    wrong = []
    with zipfile.ZipFile(wheel) as archive:
        for name in archive.namelist():
            if not name.lower().endswith((".pyd", ".dll")):
                continue
            base = name.rsplit("/", 1)[-1].lower()
            if (m := VTK_DLL.fullmatch(base)) and m["hash"]:
                wrong.append(f"{name}: VTK DLL name is mangled")
            if (m := MSVC_RUNTIME.fullmatch(base)) and not m["hash"]:
                wrong.append(f"{name}: bundled MSVC runtime is not mangled")
            pe = pefile.PE(data=archive.read(name))
            for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
                dependency = entry.dll.decode().lower()
                if (m := VTK_DLL.fullmatch(dependency)) and m["hash"]:
                    wrong.append(f"{name}: imports mangled {dependency}")
                if (m := MSVC_RUNTIME.fullmatch(dependency)) and not m["hash"]:
                    wrong.append(f"{name}: imports unmangled {dependency}")
            pe.close()
    if wrong:
        raise RuntimeError("Wheel DLL names are wrong:\n" + "\n".join(wrong))


def audit(wheel: str) -> None:
    """Run every DLL import audit on one repaired wheel."""
    audit_python_imports(wheel)
    audit_dll_names(wheel)


def main():
    if sys.argv[1] == "audit":
        for wheel in sys.argv[2:]:
            audit(wheel)
            print(f"audit: {Path(wheel).name} imports only its own Python DLL, keeps the "
                  "VTK DLL names and a private MSVC runtime", flush=True)
        return
    directory = Path(sys.argv[1]).resolve()
    wheels = {}
    for project in ("cvista", "cvista_rendering", "cvista_io"):
        matches = list(directory.glob(f"{project}-*-cp312-abi3-win_amd64.whl"))
        assert len(matches) == 1, (project, matches)
        wheel = matches[0]
        wheels[project] = wheel
        print(f"Python {sys.version}: {wheel.name} SHA256 "
              f"{hashlib.sha256(wheel.read_bytes()).hexdigest()}", flush=True)
    assert len(list(directory.glob("*.whl"))) == 3

    run("-m", "pip", "install", "numpy", "matplotlib", "pytest", "pefile")

    for wheel in wheels.values():
        audit(str(wheel))

    run("-m", "pip", "install", "--no-deps", wheels["cvista"])
    run("-c", """
import importlib.util
from cvista.vtkFiltersSources import vtkSphereSource
sphere = vtkSphereSource()
sphere.Update()
assert sphere.GetOutput().GetNumberOfCells() > 0
for module in ('vtkRenderingCore', 'vtkIOXML', 'vtkIOCore'):
    assert importlib.util.find_spec('cvista.' + module) is None, module
""")
    run("-m", "pip", "install", "--no-deps", wheels["cvista_rendering"])
    run("-c", """
import importlib.util
from cvista.vtkRenderingCore import vtkRenderer, vtkPolyDataMapper
import cvista.vtkRenderingMatplotlib
from cvista.vtkRenderingFreeType import vtkMathTextFreeTypeTextRenderer
assert vtkMathTextFreeTypeTextRenderer().MathTextIsSupported()
assert vtkRenderer() is not None
assert vtkPolyDataMapper() is not None
assert importlib.util.find_spec('cvista.vtkIOXML') is None
""")
    run("-m", "pip", "install", "--no-deps", wheels["cvista_io"])
    run("-c", """
from pathlib import Path
from tempfile import TemporaryDirectory
from cvista.vtkFiltersSources import vtkSphereSource
from cvista.vtkIOXML import vtkXMLPolyDataWriter, vtkXMLPolyDataReader
from cvista.vtkIOExport import vtkExporter
sphere = vtkSphereSource()
sphere.Update()
with TemporaryDirectory() as directory:
    filename = str(Path(directory) / 'sphere.vtp')
    writer = vtkXMLPolyDataWriter()
    writer.SetFileName(filename)
    writer.SetInputData(sphere.GetOutput())
    assert writer.Write() == 1
    reader = vtkXMLPolyDataReader()
    reader.SetFileName(filename)
    reader.Update()
    assert reader.GetOutput().GetNumberOfCells() == sphere.GetOutput().GetNumberOfCells()
""")
    root = Path(__file__).resolve().parents[1]
    run(root / "ci/smoke_min.py")
    run("-m", "pytest", "-v", "--tb=short", "-p", "no:cacheprovider",
        root / "tests/regression/test_smp_python_observer_deadlock.py")


if __name__ == "__main__":
    main()
