#!/usr/bin/env python3
"""Hooks for `pyvista-build cvista`: `version`, `check <build_dir>` and `retag <build_dir>`."""

import contextlib
import glob
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "cibw"))

import cvista_backend  # noqa: E402

OPTIONS_FILE = ".cvista-build-options.json"

USAGE = """\
usage: pyvista_build_hooks.py version            print VTK_VERSION_SUFFIX for HEAD
       pyvista_build_hooks.py check <build_dir>  fail when abi3 cannot build here or abi3/lto changed
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
            cwd=cvista_backend.REPO,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    base = cvista_backend.VTK_BASE_VERSION
    if tag == base:
        return ""
    if tag.startswith(base + "."):
        return tag[len(base) + 1 :]
    return None


def _sha_suffix() -> str:
    """Return ``dev0+g<sha8>`` for HEAD."""
    try:
        sha = subprocess.run(
            ["git", "rev-parse", "--short=8", "HEAD"],
            cwd=cvista_backend.REPO,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        sys.exit(
            "HEAD is not a release tag and git cannot name it, so the version is unknown"
        )
    return f"dev0+g{sha}"


def version() -> str:
    """Return VTK_VERSION_SUFFIX: from the exact tag, else setuptools_scm, else ``dev0+g<sha>``."""
    suffix = _tag_suffix()
    if suffix is not None:
        return suffix
    try:
        import setuptools_scm  # noqa: F401
    except ImportError:
        return _sha_suffix()
    with contextlib.redirect_stdout(sys.stderr):
        return cvista_backend._version_suffix()


def check(build_dir: str) -> None:
    """Exit when CVISTA_ABI3 cannot be built here, or when abi3 or LTO changed in this tree."""
    abi3 = _abi3_configured(build_dir)
    if abi3:
        _check_abi3()
    options = {"abi3": abi3, "lto": os.environ.get("CVISTA_LTO", "")}
    stamp = os.path.join(build_dir, OPTIONS_FILE)
    if os.path.isfile(stamp):
        with open(stamp, encoding="utf-8") as f:
            before = json.load(f)
        if before != options:
            sys.exit(
                f"abi3/lto changed since {build_dir} was configured ({before} -> {options}), "
                "and the old compiler flags stay in its CMake cache; run again with --clean"
            )
        return
    with open(stamp, "w", encoding="utf-8") as f:
        json.dump(options, f)


def _check_abi3() -> None:
    """Exit when CVISTA_ABI3 cannot be built with this Python or platform."""
    if sys.version_info < cvista_backend.ABI3_FLOOR_VERSION:
        sys.exit(
            f"CVISTA_ABI3 is ON but Python {sys.version_info[0]}.{sys.version_info[1]} is "
            f"below the stable-ABI floor 3.{cvista_backend.ABI3_FLOOR_VERSION[1]}; "
            "run again with --opt abi3=0"
        )
    if sys.platform == "win32":
        sys.exit(
            "CVISTA_ABI3 is ON but the Windows link needs LIB=<python>/libs in the "
            "build environment, which pyvista-build cannot set; run again with --opt abi3=0"
        )


def retag(build_dir: str) -> None:
    """Rename the newest wheel in <build_dir>/dist to its abi3 tag when configured."""
    if not _abi3_configured(build_dir):
        print("retag: CVISTA_ABI3 is OFF; wheel keeps its version-specific tag")
        return
    wheels = glob.glob(os.path.join(build_dir, "dist", "*.whl"))
    if not wheels:
        sys.exit(f"retag: no wheel in {build_dir}/dist")
    cvista_backend._retag_abi3(max(wheels, key=os.path.getmtime))


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
