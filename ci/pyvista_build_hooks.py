#!/usr/bin/env python3
"""Hooks for `pyvista-build cvista`: `version`, `check <build_dir>` and `retag <build_dir>`."""

import base64
import glob
import hashlib
import json
import os
import re
import subprocess
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VTK_BASE_VERSION = "9.7.0"
ABI3_FLOOR_TAG = "cp312"  # mirrors CVISTA_ABI3_VERSION 0x030c0000 in minimal.cmake
ABI3_FLOOR_VERSION = (3, 12)
OPTIONS_FILE = ".cvista-build-options.json"

USAGE = """\
usage: pyvista_build_hooks.py version            print VTK_VERSION_SUFFIX for HEAD
       pyvista_build_hooks.py check <build_dir>  fail when abi3 cannot build here or an option changed
       pyvista_build_hooks.py retag <build_dir>  give the wheel in <build_dir>/dist the abi3 tag
"""


def _abi3_configured(build_dir: str) -> bool:
    """Return the CVISTA_ABI3 value recorded in the build tree's CMake cache."""
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache):
        sys.exit(f"{build_dir} holds no CMakeCache.txt; configure first")
    with open(cache, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^CVISTA_ABI3:BOOL=(.*)$", line.strip())
            if m:
                return m.group(1).upper() in ("ON", "1", "TRUE", "YES")
    return False


def _tag_suffix() -> "str | None":
    """Return the suffix of the release tag on HEAD, or None when HEAD is not tagged."""
    try:
        tag = subprocess.run(
            ["git", "describe", "--tags", "--exact-match", "HEAD"],
            cwd=REPO,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    if tag == VTK_BASE_VERSION:
        return ""
    if tag.startswith(VTK_BASE_VERSION + "."):
        return tag[len(VTK_BASE_VERSION) + 1 :]
    return None


def _sha_suffix() -> str:
    """Return ``dev0+g<sha8>`` for HEAD."""
    try:
        sha = subprocess.run(
            ["git", "rev-parse", "--short=8", "HEAD"],
            cwd=REPO,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        sys.exit(
            "HEAD is not a release tag and git cannot name it, so the version is unknown"
        )
    return f"dev0+g{sha}"


def _scm_suffix() -> str:
    """Return the suffix setuptools_scm derives from the tags, past ``VTK_BASE_VERSION.``."""
    from setuptools_scm import get_version

    version = get_version(
        root=REPO,
        dist_name="cvista",
        version_scheme="guess-next-dev",
        local_scheme="no-local-version",
        fallback_version=f"{VTK_BASE_VERSION}.0.dev0",
    )
    if version == VTK_BASE_VERSION:
        return ""
    prefix = VTK_BASE_VERSION + "."
    if not version.startswith(prefix):
        if ".dev" in version:
            print(
                f"scm version {version} predates VTK base {VTK_BASE_VERSION}; "
                "using VTK_VERSION_SUFFIX=0.dev0",
                file=sys.stderr,
            )
            return "0.dev0"
        sys.exit(
            f"scm version {version!r} does not start with the VTK base {VTK_BASE_VERSION!r}; "
            f"tag releases as {VTK_BASE_VERSION}.N"
        )
    return version[len(prefix) :]


def version() -> str:
    """Return VTK_VERSION_SUFFIX: from the exact tag, else setuptools_scm, else ``dev0+g<sha>``."""
    suffix = _tag_suffix()
    if suffix is not None:
        return suffix
    try:
        import setuptools_scm  # noqa: F401
    except ImportError:
        return _sha_suffix()
    return _scm_suffix()


def check(build_dir: str) -> None:
    """Exit when CVISTA_ABI3 cannot be built here, or when abi3, LTO or gate_o2 changed in this tree."""
    abi3 = _abi3_configured(build_dir)
    if abi3 and sys.version_info < ABI3_FLOOR_VERSION:
        sys.exit(
            f"CVISTA_ABI3 is ON but Python {sys.version_info[0]}.{sys.version_info[1]} is "
            f"below the stable-ABI floor 3.{ABI3_FLOOR_VERSION[1]}; run again with --opt abi3=0"
        )
    options = {
        "abi3": abi3,
        "lto": os.environ.get("CVISTA_LTO", ""),
        "gate_o2": os.environ.get("CVISTA_GATE_O2") == "1",
    }
    stamp = os.path.join(build_dir, OPTIONS_FILE)
    if os.path.isfile(stamp):
        with open(stamp, encoding="utf-8") as f:
            before = json.load(f)
        if before != options:
            sys.exit(
                f"abi3/lto/gate_o2 changed since {build_dir} was configured ({before} -> {options}), "
                "and the old compiler flags stay in its CMake cache; run again with --clean"
            )
        return
    with open(stamp, "w", encoding="utf-8") as f:
        json.dump(options, f)


def retag(build_dir: str) -> None:
    """Rename the newest wheel in <build_dir>/dist to its abi3 tag when configured."""
    if not _abi3_configured(build_dir):
        print("retag: CVISTA_ABI3 is OFF; wheel keeps its version-specific tag")
        return
    wheels = glob.glob(os.path.join(build_dir, "dist", "*.whl"))
    if not wheels:
        sys.exit(f"retag: no wheel in {build_dir}/dist")
    _retag_abi3(max(wheels, key=os.path.getmtime))


def _retag_abi3(wheel: str) -> str:
    """Rename a ``cpXY-cpXY`` wheel to ``cp312-abi3`` and rewrite its WHEEL tag and RECORD."""
    name = os.path.basename(wheel)
    m = re.match(r"^(?P<base>.+)-(?P<py>[^-]+)-(?P<abi>[^-]+)-(?P<plat>[^-]+)\.whl$", name)
    if not m:
        raise RuntimeError(f"cannot parse wheel filename for abi3 retag: {name}")
    if m.group("abi") == "abi3":
        return wheel
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
                text = re.sub(
                    r"^Tag: .+$",
                    f"Tag: {new_pyabi}-{m.group('plat')}",
                    data.decode("utf-8"),
                    flags=re.MULTILINE,
                )
                data = text.encode("utf-8")
            zout.writestr(item, data)
    os.replace(tmp, new_path)
    if new_path != wheel:
        os.remove(wheel)
    _rewrite_record_for_wheel(new_path)
    print(f"retag: abi3 wheel -> {os.path.basename(new_path)}", flush=True)
    return new_path


def _rewrite_record_for_wheel(wheel: str) -> None:
    """Recompute the RECORD line of the .dist-info/WHEEL file."""
    with zipfile.ZipFile(wheel, "r") as z:
        names = z.namelist()
        wheel_name = next(n for n in names if n.endswith(".dist-info/WHEEL"))
        record_name = next(n for n in names if n.endswith(".dist-info/RECORD"))
        wheel_data = z.read(wheel_name)
        record_text = z.read(record_name).decode("utf-8")
        contents = {n: z.read(n) for n in names}

    digest = base64.urlsafe_b64encode(hashlib.sha256(wheel_data).digest()).rstrip(b"=").decode()
    new_line = f"{wheel_name},sha256={digest},{len(wheel_data)}"
    lines = [
        new_line if line.startswith(wheel_name + ",") else line
        for line in record_text.splitlines()
    ]
    contents[record_name] = ("\n".join(lines) + "\n").encode("utf-8")

    tmp = wheel + ".rec.tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for n, data in contents.items():
            z.writestr(n, data)
    os.replace(tmp, wheel)


def main(argv: "list[str]") -> int:
    if argv == ["version"]:
        print(version())
        return 0
    if len(argv) == 2 and argv[0] == "check":
        check(argv[1])
        return 0
    if len(argv) == 2 and argv[0] == "retag":
        retag(argv[1])
        return 0
    print(USAGE, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
