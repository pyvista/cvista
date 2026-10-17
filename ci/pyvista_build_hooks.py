#!/usr/bin/env python3
"""Hooks for `pyvista build cvista`: `version`, `check <build_dir>` and `retag <build_dir>`."""

import contextlib
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "cibw"))

import cvista_backend  # noqa: E402

USAGE = """\
usage: pyvista_build_hooks.py version            print VTK_VERSION_SUFFIX for HEAD
       pyvista_build_hooks.py check <build_dir>  fail when CVISTA_ABI3 cannot build here
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


def version() -> str:
    """Return VTK_VERSION_SUFFIX: from the exact tag, else from setuptools_scm."""
    suffix = _tag_suffix()
    if suffix is not None:
        return suffix
    try:
        import setuptools_scm  # noqa: F401
    except ImportError:
        sys.exit(
            "HEAD is not a release tag and setuptools_scm is not installed, so the "
            "version cannot be derived; pip install 'setuptools_scm>=8' and run again"
        )
    with contextlib.redirect_stdout(sys.stderr):
        return cvista_backend._version_suffix()


def check(build_dir: str) -> None:
    """Exit when the configured CVISTA_ABI3 cannot be built with this Python."""
    if not _abi3_configured(build_dir):
        return
    if sys.version_info < cvista_backend.ABI3_FLOOR_VERSION:
        sys.exit(
            f"CVISTA_ABI3 is ON but Python {sys.version_info[0]}.{sys.version_info[1]} is "
            f"below the stable-ABI floor 3.{cvista_backend.ABI3_FLOOR_VERSION[1]}; "
            "run again with --opt abi3=0"
        )
    if sys.platform == "win32":
        sys.exit(
            "CVISTA_ABI3 is ON but the Windows link needs LIB=<python>/libs in the "
            "build environment, which pyvista build cannot set; run again with --opt abi3=0"
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
