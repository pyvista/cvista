"""PEP 517 backend that forwards `pip wheel .` to `pyvista-build cvista`."""

from __future__ import annotations

import glob
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_requires_for_build_wheel(config_settings=None):
    """Return nothing: `[build-system] requires` already installs pyvista-build."""
    return []


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    """Build the wheel with `pyvista-build cvista` and copy it into `wheel_directory`."""
    build_dir = os.environ.get("CVISTA_BUILD_DIR", os.path.join(REPO, "build-pyvista"))
    cmd = [
        sys.executable,
        "-m",
        "pyvista_build",
        "cvista",
        "--yes",
        "--source",
        REPO,
        "--build-dir",
        build_dir,
        "--python",
        sys.executable,
    ]
    subprocess.run(cmd, check=True)
    wheels = glob.glob(os.path.join(build_dir, "wheelhouse", "*.whl"))
    if not wheels:
        raise RuntimeError(f"pyvista-build produced no wheel in {build_dir}/wheelhouse")
    wheel = max(wheels, key=os.path.getmtime)
    shutil.copy2(wheel, os.path.join(wheel_directory, os.path.basename(wheel)))
    return os.path.basename(wheel)


def build_sdist(sdist_directory, config_settings=None):
    """Refuse: the wheel is generated from the CMake build tree, so there is no sdist."""
    raise RuntimeError("cvista has no sdist; build the wheel with `pyvista-build cvista` or `pip wheel .`")
