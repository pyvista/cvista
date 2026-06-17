"""PEP 517 in-tree build backend that drives fvtk's cmake -> build -> generated
build-tree ``setup.py bdist_wheel`` flow, so ``pip wheel .`` (and therefore
cibuildwheel) can build the wheel.

fvtk is NOT a normal ``pip install .`` project: VTK only emits a ``setup.py``
INSIDE the cmake build tree (VTK_WHEEL_BUILD), after a full C++ configure+build.
This backend bridges that gap:

  build_wheel():
    1. cmake -S <repo> -B <build> -C ci/cmake/linux.cmake  (the proven init-cache)
    2. cmake --build <build> --parallel
    3. ci/prune_setup_py.py <build>   (strip dead UI subpackages)
    4. cd <build> && python setup.py bdist_wheel
    5. copy the produced wheel into the directory pip asked for

It implements only the hooks pip needs to produce a wheel (build_wheel +
get_requires_for_build_wheel + a degenerate build_sdist). No editable install.

Knobs (env):
  FVTK_BUILD_DIR        cmake build tree (default <repo>/build-cibw); kept
                        between python legs so ccache + the configured tree are
                        reused (only the python-version wrappers recompile).
  FVTK_BUILD_JOBS       cmake --build --parallel N (default: os.cpu_count()).
  FVTK_CMAKE_INIT       init-cache file (default ci/cmake/linux.cmake).
  CMAKE_C/CXX_COMPILER_LAUNCHER  honoured by cmake (e.g. ccache) if exported.
  FVTK_LTO etc.         consumed by the init-cache exactly as in the raw build.
"""
from __future__ import annotations

import glob
import os
import re
import shutil
import subprocess
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# TWO-WHEEL scheme (PR #16). The abi3 stable-ABI floor is CPython 3.12
# (Py_LIMITED_API 0x030c0000): PyMemberDef + the Py_T_*/Py_READONLY member
# constants the heap-type wrappers emit only entered the stable ABI in 3.12
# (gh-93274), so 3.11 cannot be a stable-ABI target. The matrix is therefore:
#   * cp311  -> a STATIC per-version wheel (FVTK_ABI3 forced OFF, normal
#               cp311-cp311 tag). 3.11 IS supported, just not via abi3.
#   * cp312+ -> ONE abi3 wheel tagged cp312-abi3 (loads on CPython 3.12+).
#               cibuildwheel's abi3 dedup reuses it for cp313/cp314, so the
#               cp312-* cp313-* cp314-* legs yield a single build+wheel.
# The decision is made PER LEG from the running build python's version (below),
# NOT from a global on/off — that is what lets the same backend emit a static
# cp311 wheel and an abi3 cp312 wheel in one cibuildwheel matrix run.
#
# Escape hatch: FVTK_ABI3=0 in the env forces the legacy static-type wheel for
# ANY leg (incl. cp312+), restoring strict byte-for-byte parity incl. __flags__.
ABI3_FLOOR_TAG = "cp312"  # mirrors FVTK_ABI3_VERSION 0x030c0000 in minimal.cmake
ABI3_FLOOR_VERSION = (3, 12)  # mirrors 0x030c0000


def _abi3_enabled() -> bool:
    """Whether THIS leg should build the abi3 (stable-ABI) wheel.

    True iff the build python is >= the abi3 floor (3.12) AND the FVTK_ABI3
    escape hatch is not set to 0. On a 3.11 leg this is False, so 3.11 builds a
    normal static per-version wheel (the stable ABI has no PyMemberDef < 3.12)."""
    if os.environ.get("FVTK_ABI3", "1") == "0":
        return False
    return sys.version_info[:2] >= ABI3_FLOOR_VERSION


def _build_dir() -> str:
    # Per-python build tree: the generated wrappers + setup.py are ABI-specific.
    # All legs share the python-independent C++ kit objects via ccache
    # (CMAKE_*_COMPILER_LAUNCHER), so only the first leg pays the full C++ cost.
    # The abi3 legs (cp312+) share ONE "abi3" tree (the wrappers are version-
    # independent under Py_LIMITED_API); the static cp311 leg gets its own
    # SOABI-keyed tree so its per-version wrappers don't clobber the abi3 ones.
    base = os.environ.get("FVTK_BUILD_DIR", os.path.join(REPO, "build-cibw"))
    if _abi3_enabled():
        return f"{base}-abi3"
    import sysconfig

    tag = sysconfig.get_config_var("SOABI") or f"py{sys.version_info[0]}{sys.version_info[1]}"
    return f"{base}-{tag}"


def _jobs() -> str:
    return os.environ.get("FVTK_BUILD_JOBS", str(os.cpu_count() or 4))


def _init_cache() -> str:
    # FVTK_CMAKE_INIT selects the per-OS init-cache (ci/cmake/{linux,macos,
    # windows}.cmake). It is set in pyproject [tool.cibuildwheel.<os>.environment]
    # as a REPO-relative path (e.g. "ci/cmake/windows.cmake"): cibuildwheel does
    # NOT expand its {project} token inside environment values (only in
    # before-build/test-command), so an absolute "{project}/..." would reach cmake
    # literally and fail ("Not a file: .../{project}/..."). Resolve a relative
    # value against REPO here; absolute values are passed through unchanged.
    val = os.environ.get("FVTK_CMAKE_INIT")
    if not val:
        return os.path.join(REPO, "ci", "cmake", "linux.cmake")
    return val if os.path.isabs(val) else os.path.join(REPO, val)


def _run(cmd, cwd=None):
    print("+ " + " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def _configure_and_build():
    build = _build_dir()
    launcher_c = os.environ.get("CMAKE_C_COMPILER_LAUNCHER", "")
    launcher_cxx = os.environ.get("CMAKE_CXX_COMPILER_LAUNCHER", "")
    cfg = [
        "cmake",
        "-S",
        REPO,
        "-B",
        build,
        "-G",
        "Ninja",
        "-C",
        _init_cache(),
        f"-DPython3_EXECUTABLE={sys.executable}",
        "-DPython3_FIND_STRATEGY=LOCATION",
        # Keep the backend's abi3 view and cmake's in lockstep: minimal.cmake
        # defaults FVTK_ABI3 ON, and FVTK_ABI3=0 in the env flips both off.
        f"-DFVTK_ABI3={'ON' if _abi3_enabled() else 'OFF'}",
    ]
    if launcher_c:
        cfg.append(f"-DCMAKE_C_COMPILER_LAUNCHER={launcher_c}")
    if launcher_cxx:
        cfg.append(f"-DCMAKE_CXX_COMPILER_LAUNCHER={launcher_cxx}")
    _run(cfg)
    _run(["cmake", "--build", build, "--parallel", _jobs()])
    return build


def _bdist_wheel(build: str) -> str:
    _run([sys.executable, os.path.join(REPO, "ci", "prune_setup_py.py"), build])
    # Clean any stale wheel so we can identify the fresh one unambiguously.
    dist = os.path.join(build, "dist")
    if os.path.isdir(dist):
        for w in glob.glob(os.path.join(dist, "*.whl")):
            os.remove(w)
    _run([sys.executable, "setup.py", "bdist_wheel"], cwd=build)
    wheels = glob.glob(os.path.join(dist, "*.whl"))
    if not wheels:
        raise RuntimeError(f"no wheel produced in {dist}")
    return max(wheels, key=os.path.getmtime)


def _retag_abi3(wheel: str) -> str:
    """Rewrite a version-tagged wheel (e.g. ...-cp312-cp312-linux_x86_64.whl) into
    the stable-ABI form ...-cp312-abi3-linux_x86_64.whl: the generated build-tree
    setup.py has no notion of Py_LIMITED_API, so it tags the wheel with the build
    python's version even though the modules are abi3 (vtkXxx.abi3.so). We flip the
    python tag to the floor (cp312), the ABI tag to abi3, rewrite the WHEEL `Tag:`
    line + the RECORD entry for it, and rename the file so the result installs on
    CPython 3.12+. Bit-exact: only the wheel METADATA tag changes, not any module.
    """
    name = os.path.basename(wheel)
    m = re.match(r"^(?P<base>.+)-(?P<py>[^-]+)-(?P<abi>[^-]+)-(?P<plat>[^-]+)\.whl$", name)
    if not m:
        raise RuntimeError(f"cannot parse wheel filename for abi3 retag: {name}")
    if m.group("abi") == "abi3":
        return wheel  # already abi3-tagged
    new_pyabi = f"{ABI3_FLOOR_TAG}-abi3"
    new_name = f"{m.group('base')}-{new_pyabi}-{m.group('plat')}.whl"
    new_path = os.path.join(os.path.dirname(wheel), new_name)

    tmp = wheel + ".retag.tmp"
    with zipfile.ZipFile(wheel, "r") as zin, zipfile.ZipFile(
        tmp, "w", zipfile.ZIP_DEFLATED
    ) as zout:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.filename.endswith(".dist-info/WHEEL"):
                # Replace the whole py-abi-plat triple on every `Tag:` line with
                # the abi3 form (one Tag line in practice; loop is robust to more).
                text = data.decode("utf-8")
                text = re.sub(
                    r"^Tag: .+$",
                    f"Tag: {new_pyabi}-{m.group('plat')}",
                    text,
                    flags=re.MULTILINE,
                )
                data = text.encode("utf-8")
            zout.writestr(item, data)
    os.replace(tmp, new_path)
    if new_path != wheel:
        os.remove(wheel)
    # Fix the RECORD entry for WHEEL (its hash/size changed).
    _rewrite_record_for_wheel(new_path)
    print(f"fvtk_backend: retagged abi3 wheel -> {os.path.basename(new_path)}", flush=True)
    return new_path


def _rewrite_record_for_wheel(wheel: str) -> None:
    """Recompute the RECORD line for the .dist-info/WHEEL file after we edited it."""
    import base64
    import hashlib

    with zipfile.ZipFile(wheel, "r") as z:
        names = z.namelist()
        wheel_name = next(n for n in names if n.endswith(".dist-info/WHEEL"))
        record_name = next(n for n in names if n.endswith(".dist-info/RECORD"))
        wheel_data = z.read(wheel_name)
        record_text = z.read(record_name).decode("utf-8")
        contents = {n: z.read(n) for n in names}

    digest = base64.urlsafe_b64encode(hashlib.sha256(wheel_data).digest()).rstrip(b"=").decode()
    new_line = f"{wheel_name},sha256={digest},{len(wheel_data)}"
    lines = []
    for line in record_text.splitlines():
        if line.startswith(wheel_name + ","):
            lines.append(new_line)
        else:
            lines.append(line)
    contents[record_name] = ("\n".join(lines) + "\n").encode("utf-8")

    tmp = wheel + ".rec.tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for n, data in contents.items():
            z.writestr(n, data)
    os.replace(tmp, wheel)


# --- PEP 517 hooks -----------------------------------------------------------


def get_requires_for_build_wheel(config_settings=None):
    # pip builds the wheel in an ISOLATED env (build isolation), so cmake + ninja
    # must be declared here or the `cmake`/`ninja` binaries are absent from the
    # backend's PATH (CIBW_BEFORE_BUILD installs them into the OUTER python, not
    # pip's isolated build env). pip's cmake>=3.22 wheel ships the binary; ninja
    # >=1.11 is needed for VTK's multiple-output wrapping edges. setuptools+wheel
    # are what the generated build-tree setup.py needs. auditwheel repair is done
    # by cibuildwheel afterwards.
    return ["cmake>=3.22,<4.2", "ninja>=1.11", "setuptools<81", "wheel"]


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    build = _configure_and_build()
    wheel = _bdist_wheel(build)
    if _abi3_enabled():
        wheel = _retag_abi3(wheel)
    dest = os.path.join(wheel_directory, os.path.basename(wheel))
    shutil.copy2(wheel, dest)
    print(f"fvtk_backend: produced {dest}", flush=True)
    return os.path.basename(wheel)


def build_sdist(sdist_directory, config_settings=None):
    raise RuntimeError(
        "fvtk has no sdist: the wheel is generated from the cmake build tree. "
        "Build wheels directly (pip wheel . / cibuildwheel)."
    )


# prepare_metadata_for_build_wheel is intentionally omitted: pip falls back to
# building the full wheel to obtain metadata, which is what we want (the metadata
# only exists after cmake generates setup.py).
