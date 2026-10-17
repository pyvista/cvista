#!/usr/bin/env python3
"""Hooks for `pyvista build cvista`: `version` prints VTK_VERSION_SUFFIX, `retag <build_dir>` abi3-tags the wheel."""

import glob
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "cibw"))

import cvista_backend  # noqa: E402


def _abi3_configured(build_dir: str) -> bool:
    cache = os.path.join(build_dir, "CMakeCache.txt")
    with open(cache, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^CVISTA_ABI3:BOOL=(.*)$", line.strip())
            if m:
                return m.group(1).upper() in ("ON", "1", "TRUE", "YES")
    return False


def main(argv: list[str]) -> int:
    if argv[:1] == ["version"]:
        print(cvista_backend._version_suffix())
        return 0
    if argv[:1] == ["retag"] and len(argv) == 2:
        build_dir = argv[1]
        if not _abi3_configured(build_dir):
            print("retag: CVISTA_ABI3 is OFF; wheel keeps its version-specific tag")
            return 0
        wheels = glob.glob(os.path.join(build_dir, "dist", "*.whl"))
        if not wheels:
            print(f"retag: no wheel in {build_dir}/dist", file=sys.stderr)
            return 1
        cvista_backend._retag_abi3(max(wheels, key=os.path.getmtime))
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
