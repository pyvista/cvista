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


def main():
    if sys.argv[1] == "audit":
        for wheel in sys.argv[2:]:
            audit_python_imports(wheel)
            print(f"audit: {Path(wheel).name} imports only its own Python DLL", flush=True)
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
        audit_python_imports(str(wheel))

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
