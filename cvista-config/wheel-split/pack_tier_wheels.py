#!/usr/bin/env python3
"""pack_tier_wheels.py — pack the partitioned tier trees into 3 installable wheels.

Input : /tmp/tiers/{core,rendering,io}/cvista  (produced by partition_wheels.py)
Output: 3 wheels in <outdir> — cvista, cvista-rendering, cvista-io — all owning files
        in the shared `cvista/` import package (no file overlap; core owns __init__.py).
        rendering/io declare `Requires-Dist: cvista==<ver>` so ABIs never mismatch.
"""
import os, re, sys, subprocess, shutil
from pathlib import Path

# Derived from the actual built wheel by CI (see the split job). The abi3 build
# produces e.g. VER=9.6.2.0 / PYTAG=cp312-abi3-manylinux_2_28_x86_64. Fall back to
# dev values for a local run.
VER = os.environ.get("CVISTA_WHEEL_VERSION", "9.7.0.dev0")
PYTAG = os.environ.get("CVISTA_WHEEL_TAG", "cp312-abi3-manylinux_2_28_x86_64")
TIERS = {
    "core":      ("cvista",           []),
    # The core renderer (OpenGL2/FreeType/Charts/Views) does NOT link io, so
    # cvista-rendering requires only cvista. The bridge modules that DO need io
    # (scene import/export, molecule rendering) are lazy: absent io, only those
    # names raise an install-io ImportError, so io stays an optional extra
    # (cvista-rendering[io]) rather than a hard dependency.
    "rendering": ("cvista_rendering", [f"cvista=={VER}"]),
    "io":        ("cvista_io",        [f"cvista=={VER}"]),
}
SUMMARY = {
    "core": "cvista core: VTK Common/Filters/Imaging compute kernels (rendering-free, IO-free, offline).",
    "rendering": "cvista rendering tier (OpenGL2/FreeType/Charts/Views). Requires cvista; cvista-io optional (scene import/export, molecule rendering).",
    "io": "cvista IO tier: every VTK reader/writer (XML/legacy/PLY/image/HDF/Exodus/...). Requires cvista.",
}

# The long description shown on each PyPI project page. Without it PyPI reports
# "The author of this package has not provided a project description".
_INTRO = (
    "cvista is a fast, lean fork of [VTK](https://gitlab.kitware.com/vtk/vtk) 9.6.2, "
    "maintained by the [PyVista](https://github.com/pyvista) community as a drop-in "
    "graphics layer for PyVista. It is byte-for-byte identical to stock VTK 9.6.2 by "
    "default, BSD-3 licensed, and developed in the open at "
    "[pyvista/cvista](https://github.com/pyvista/cvista). It is not affiliated with Kitware.\n\n"
    "cvista is published as three tiers that share the `cvista` import package:\n\n"
    "- `cvista` core: VTK Common/Filters/Imaging compute kernels (rendering-free, IO-free)\n"
    "- `cvista-rendering`: the OpenGL2/FreeType/Charts/Views rendering stack\n"
    "- `cvista-io`: every VTK reader and writer\n"
)
_LINKS = (
    "## Links\n\n"
    "- Source and issues: https://github.com/pyvista/cvista\n"
    "- PyVista: https://github.com/pyvista/pyvista\n"
)
_TIER_BODY = {
    "core": (
        "## This package: cvista\n\n"
        "The base tier. It provides VTK's Common, Filters, and Imaging modules: the data "
        "model and compute kernels, with no rendering and no data IO. Install it alone for "
        "offline geometry and array processing, or opt into the other tiers with the "
        "`rendering`, `io`, and `all` extras.\n\n"
        "```\npip install cvista            # core only\n"
        "pip install cvista[rendering] # + rendering tier\n"
        "pip install cvista[io]        # + data IO tier\n"
        "pip install cvista[all]       # everything\n```\n"
    ),
    "rendering": (
        "## This package: cvista-rendering\n\n"
        "Adds VTK's rendering stack (OpenGL2, FreeType, Charts, Views) on top of the cvista "
        "core, and requires `cvista`. The bridge modules that also need data IO (scene "
        "import/export, molecule rendering) are optional: install `cvista-rendering[io]` to "
        "enable them, otherwise those names raise an install-io error while the rest of the "
        "renderer works.\n\n"
        "```\npip install cvista-rendering\npip install cvista-rendering[io]  # + IO bridge modules\n```\n"
    ),
    "io": (
        "## This package: cvista-io\n\n"
        "Adds VTK's full set of readers and writers (XML, legacy, PLY, image formats, HDF, "
        "Exodus, and more) on top of the cvista core, and requires `cvista`.\n\n"
        "```\npip install cvista-io\n```\n"
    ),
}


def description(tier, dist):
    return f"# {dist.replace('_', '-')}\n\n{_INTRO}\n{_TIER_BODY[tier]}\n{_LINKS}"

# Requires-Python must be derived from the wheel's own python tag, NOT hardcoded:
# pip filters candidate FILES on their per-file requires-python (PEP 503
# data-requires-python), so stamping the abi3 floor (>=3.12) onto the legacy
# cp310/cp311 tier wheels made those wheels unreachable — a 3.10/3.11 resolver
# skipped them despite the cpython tag matching. cp312-abi3 -> >=3.12,
# cp310-cp310 -> >=3.10, etc.
_m = re.match(r"cp(\d)(\d+)-", PYTAG)
if not _m:
    sys.exit(f"cannot derive Requires-Python floor from wheel tag {PYTAG!r}")
REQUIRES_PYTHON = f">={_m.group(1)}.{_m.group(2)}"

def metadata(tier, dist, reqs):
    lines = [
        "Metadata-Version: 2.1",
        f"Name: {dist.replace('_','-')}",
        f"Version: {VER}",
        f"Summary: {SUMMARY[tier]}",
        "Description-Content-Type: text/markdown",
        "Home-page: https://github.com/pyvista/cvista",
        "License: BSD-3-Clause",
        f"Requires-Python: {REQUIRES_PYTHON}",
    ]
    for r in reqs:
        lines.append(f"Requires-Dist: {r}")
    # The CORE distribution exposes optional extras so users can opt into tiers:
    #   pip install cvista              -> core only (offline)
    #   pip install cvista[rendering]   -> + rendering tier
    #   pip install cvista[io]          -> + heavy data IO tier
    #   pip install cvista[all]         -> everything (full functionality)
    if dist == "cvista":
        for extra, deps in (("rendering",[f"cvista-rendering=={VER}"]),
                            ("io",[f"cvista-io=={VER}"]),
                            ("all",[f"cvista-rendering=={VER}", f"cvista-io=={VER}"])):
            lines.append(f"Provides-Extra: {extra}")
            for d in deps:
                lines.append(f'Requires-Dist: {d}; extra == "{extra}"')
    # cvista-rendering[io] opts into the io tier for the bridge modules (scene
    # import/export, molecule rendering) that link it. Plain cvista-rendering
    # renders without io; the bridge names raise an install-io ImportError.
    if dist == "cvista_rendering":
        lines.append("Provides-Extra: io")
        lines.append(f'Requires-Dist: cvista-io=={VER}; extra == "io"')
    # Headers, one blank line, then the long-description payload (Metadata 2.1).
    return "\n".join(lines) + "\n\n" + description(tier, dist) + "\n"

WHEEL = (f"Wheel-Version: 1.0\nGenerator: cvista-partition\nRoot-Is-Purelib: false\n"
         f"Tag: {PYTAG}\n")

def main(srcroot, outdir):
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    built = []
    for tier,(dist,reqs) in TIERS.items():
        stage = Path(f"/tmp/_pack_{tier}");
        if stage.exists(): shutil.rmtree(stage)
        stage.mkdir(parents=True)
        # copy the whole tier root — cvista/ AND any vendored-runtime dir the platform
        # repair created (auditwheel cvista.libs/, delocate cvista/.dylibs/, delvewheel
        # cvista.libs/). partition_wheels.py assigns each vendored lib to a tier, so a
        # tier may or may not carry a cvista.libs/ (core carries libgomp etc.).
        src_tier = Path(srcroot)/tier
        for item in sorted(src_tier.iterdir()):
            dst = stage/item.name
            if item.is_dir():
                shutil.copytree(item, dst)
            else:
                shutil.copy2(item, dst)
        di = stage/f"{dist}-{VER}.dist-info"; di.mkdir()
        (di/"METADATA").write_text(metadata(tier, dist, reqs))
        (di/"WHEEL").write_text(WHEEL)
        (di/"top_level.txt").write_text("cvista\n")
        # pack (regenerates RECORD)
        subprocess.check_call([sys.executable,"-m","wheel","pack",str(stage),"-d",str(out)],
                              stdout=subprocess.DEVNULL)
        whl = next(out.glob(f"{dist}-{VER}-*.whl"))
        built.append((tier, whl, whl.stat().st_size))
        shutil.rmtree(stage)
    for tier,whl,sz in built:
        print(f"  {tier:9s} {whl.name}  ({sz/1e6:.1f} MB)")

if __name__=="__main__":
    main(sys.argv[1] if len(sys.argv)>1 else "/tmp/tiers",
         sys.argv[2] if len(sys.argv)>2 else "/tmp/tier-wheels")
