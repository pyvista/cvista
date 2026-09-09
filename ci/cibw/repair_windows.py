#!/usr/bin/env python3
"""Windows delvewheel repair wrapper that points delvewheel at cvista's build-tree
DLL directory.

WHY THIS EXISTS
---------------
cvista is built with ``VTK_ENABLE_KITS=ON`` (cvista-config/minimal.cmake): most VTK
modules are folded into a handful of larger "kit" shared libraries (vtkCommon,
vtkFilters, vtkRendering, vtkOpenGL, vtkImaging, vtkInteraction, vtkViews,
vtkParallel, vtkIO). A kit member's per-module DLL is NOT built — its
Python-wrapper ``.pyd`` links the *kit* target (CMake/vtkModule.cmake makes the
per-module real target an INTERFACE lib whose implementation is the kit), so the
``.pyd`` import table names the kit DLL, never a per-module DLL. Modules WITHOUT
a ``KIT`` directive (e.g. ChartsCore) build as their own standalone DLL. Either
way every DLL named in a ``.pyd`` import table is a REAL file that exists in the
build tree.

VTK's wheel layout (CMake/vtkWheelPreparation.cmake) deliberately does NOT copy
those DLLs next to the ``.pyd``s on Windows ("Defaults are fine; handled by
delvewheel") — the DLLs land in ``<build_dir>/bin`` (CMakeLists.txt:277,
``CMAKE_RUNTIME_OUTPUT_DIRECTORY = <build>/${CMAKE_INSTALL_BINDIR}``), and
``delvewheel repair`` is expected to find them and bundle them into the wheel.

In the cibuildwheel flow that build ``bin`` dir is not on PATH when delvewheel
runs, so delvewheel reports the first DLL it can't resolve
(``Unable to find library: vtkchartscore-9.6.2.dll``). This is purely a SEARCH
PATH problem — NOT a kit/per-module name mismatch — so ``--add-path <bin>`` over
the build tree resolves ALL of them (kit DLLs + standalone module DLLs +
vendored third-party DLLs) in one shot.

Only the current wheel's build tree may supply DLLs. Searching all Python
build trees can bundle a same-named DLL from an earlier interpreter: a cp311
wheel then imports python310.dll even though its wrapper imports python311.dll.

USAGE (from pyproject [tool.cibuildwheel.windows] repair-wheel-command):
    python ci/cibw/repair_windows.py . {dest_dir} {wheel}

cibuildwheel does NOT substitute ``{project}`` in repair-wheel-command (only
``{wheel}``/``{dest_dir}``), but it runs the command with cwd == the project
root, so the command passes ``.`` as the project arg and this script resolves it
to an absolute path (and defensively falls back to cwd if an unsubstituted
``{project}`` placeholder ever reaches it).
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import zipfile


def _bin_dirs(project: str, wheel: str) -> list[str]:
    """Select the DLL directory belonging to this wheel's Python ABI."""
    match = re.search(
        r"-(cp\d+)-(abi3|cp\d+)-(win_amd64|win_arm64|win32)\.whl$",
        os.path.basename(wheel),
    )
    if match is None:
        raise ValueError(f"Unsupported Windows wheel filename: {wheel}")
    python_tag, abi_tag, platform_tag = match.groups()
    base = os.environ.get("CVISTA_BUILD_DIR", os.path.join(project, "build-cibw"))
    if abi_tag == "abi3" and os.environ.get("CVISTA_BUILD_DIR_PER_ABI") != "1":
        suffix = "abi3"
    else:
        suffix = f"{python_tag}-{platform_tag}"
    directory = os.path.abspath(f"{base}-{suffix}/bin")
    if not os.path.isdir(directory):
        raise FileNotFoundError(f"No DLL directory for this wheel: {directory}")
    return [directory]


def audit_python_imports(wheel: str) -> None:
    """Reject repaired wheels that import another interpreter's Python DLL."""
    import pefile

    match = re.search(r"-(cp\d+)-(abi3|cp\d+)-win", os.path.basename(wheel))
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
        raise RuntimeError(
            "Wheel imports the wrong Python runtime:\n" + "\n".join(wrong)
        )


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(
            "usage: repair_windows.py <project> <dest_dir> <wheel>",
            file=sys.stderr,
        )
        return 2
    project, dest_dir, wheel = argv[0], argv[1], argv[2]
    # cibuildwheel runs repair-wheel-command with cwd == project root and does
    # not expand {project}; "." (or a stray unsubstituted "{project}") resolves
    # to that cwd.
    if not project or "{" in project:
        project = os.getcwd()
    project = os.path.abspath(project)

    bin_dirs = _bin_dirs(project, wheel)

    cmd = ["delvewheel", "repair", "-w", dest_dir]
    for d in bin_dirs:
        cmd += ["--add-path", d]
    cmd.append(wheel)

    print("repair_windows: build DLL dirs:", flush=True)
    for d in bin_dirs:
        print(f"  - {d}", flush=True)
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)
    repaired = os.path.join(dest_dir, os.path.basename(wheel))
    audit_python_imports(repaired)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
