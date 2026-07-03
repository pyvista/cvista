#!/usr/bin/env python3
"""pack_tier_wheels.py — pack the partitioned tier trees into 3 installable wheels.

Input : /tmp/tiers/{core,rendering,io}/cvista  (produced by partition_wheels.py)
Output: 3 wheels in <outdir> — cvista, cvista-rendering, cvista-io — all owning files
        in the shared `cvista/` import package (no file overlap; core owns __init__.py).
        rendering/io declare `Requires-Dist: cvista==<ver>` so ABIs never mismatch.
"""
import os, sys, subprocess, shutil
from pathlib import Path

# Derived from the actual built wheel by CI (see the split job). The abi3 build
# produces e.g. VER=9.6.2.0 / PYTAG=cp312-abi3-manylinux_2_28_x86_64. Fall back to
# dev values for a local run.
VER = os.environ.get("CVISTA_WHEEL_VERSION", "9.6.2.dev0")
PYTAG = os.environ.get("CVISTA_WHEEL_TAG", "cp312-abi3-manylinux_2_28_x86_64")
TIERS = {
    "core":      ("cvista",           []),
    "rendering": ("cvista_rendering", [f"cvista=={VER}"]),
    "io":        ("cvista_io",        [f"cvista=={VER}"]),
}
SUMMARY = {
    "core": "cvista core: VTK Common/Filters/Imaging + native-format IO (rendering-free, offline).",
    "rendering": "cvista rendering tier (OpenGL2/FreeType/Charts/Views). Requires cvista.",
    "io": "cvista heavy data IO tier (HDF/Exodus/EnSight/NetCDF/...). Requires cvista.",
}

def metadata(dist, reqs):
    lines = [
        "Metadata-Version: 2.1",
        f"Name: {dist.replace('_','-')}",
        f"Version: {VER}",
        f"Summary: {SUMMARY[[k for k,v in TIERS.items() if v[0]==dist][0]]}",
        "Requires-Python: >=3.12",  # abi3 floor cp312 (matches the built wheel)
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
    return "\n".join(lines) + "\n"

WHEEL = (f"Wheel-Version: 1.0\nGenerator: cvista-partition\nRoot-Is-Purelib: false\n"
         f"Tag: {PYTAG}\n")

def main(srcroot, outdir):
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    built = []
    for tier,(dist,reqs) in TIERS.items():
        stage = Path(f"/tmp/_pack_{tier}");
        if stage.exists(): shutil.rmtree(stage)
        stage.mkdir(parents=True)
        # copy cvista/ payload
        shutil.copytree(Path(srcroot)/tier/"cvista", stage/"cvista")
        di = stage/f"{dist}-{VER}.dist-info"; di.mkdir()
        (di/"METADATA").write_text(metadata(dist, reqs))
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
