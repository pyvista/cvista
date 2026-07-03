#!/usr/bin/env python3
"""partition_wheels.py — split one full cvista wheel into core/rendering/io tiers.

Input : a built full cvista wheel (kits on, with the VTK::IOExtra kit split).
Output: three wheels — cvista (core), cvista-rendering, cvista-io — that share the
        cvista/ import namespace and resolve each other's libs via $ORIGIN RUNPATH.

Classification:
  * vtk<Module>.abi3.so + matching .py  -> MODULE_TIER table (by module name / prefix)
  * libvtk<Kit>-cvista.so               -> KIT_TIER table (by kit lib basename)
  * third-party libvtk<dep>-cvista.so   -> link-graph closure: lowest tier whose
                                           kept libs DT_NEEDED it; default core.
  * package scaffolding (__init__.py, *.py, util/, py.typed, numpy_*) -> core
"""
import os, re, sys, shutil, subprocess, json
from pathlib import Path

SUFFIX = "-cvista"  # VTK_CUSTOM_LIBRARY_SUFFIX

# ---- kit lib basename (without lib/.so/suffix) -> tier --------------------
KIT_TIER = {
    "vtkCommon": "core", "vtkFilters": "core", "vtkImaging": "core", "vtkIO": "core",
    "vtkRendering": "rendering", "vtkInteraction": "rendering",
    "vtkChartsCore": "rendering", "vtkViews": "rendering",
    "vtkIOExtra": "io",
    # After the tiered build disables IOParallel/IOParallelXML/IOCGNSReader, the
    # VTK::Parallel kit holds ONLY core modules (FiltersParallel/Extraction/FlowPaths/
    # ParallelCore/ParallelDIY/ParallelDIY2) -> core tier.
    "vtkParallel": "core",
}

PATCHELF = shutil.which("patchelf") or "patchelf"  # from PATH (CI + Nix); no hardcoded store path

# ---- python module name prefix -> tier (longest prefix wins) ---------------
# Only modules NOT covered by a kit lib need an entry; kit-resident modules ride
# their kit lib, but their .abi3.so wrapper still needs a tier (same as the kit).
def module_tier(mod):
    m = mod
    # rendering tier modules
    if re.match(r'vtk(Rendering|Interaction|Charts|Views|Web|DomainsChemistry)', m):
        return "rendering"
    if m in ("vtkPythonContext2D",):           # Context2D python glue -> rendering
        return "rendering"
    if m == "vtkFiltersHybridRendering":       # the 3 view-classes split out of FiltersHybrid
        return "rendering"
    # NOTE: vtkIOGeometry is NO LONGER here — #168 relocated its only rendering-
    # coupled classes (vtkGLTFReader/vtkGLTFTexture) into vtkIOImport, so IOGeometry
    # is now rendering-free and rides the core VTK::IO kit. vtkIOImport carries them
    # and stays rendering.
    if m in ("vtkIOExport","vtkIOExportGL2PS","vtkIOImport","vtkIOMINC",
             "vtkIOChemistry","vtkInfovisCore","vtkInfovisLayout"):
        return "rendering"
    # io tier modules
    if m in ("vtkIOHDF","vtkIOExodus","vtkIOEnSight","vtkIOInfovis","vtkIONetCDF",
             "vtkIOVeraOut","vtkIOSegY","vtkIOFLUENTCFF","vtkIOCGNSReader",
             "vtkIOParallel","vtkIOParallelXML","vtkIOParallelExodus","vtkIOCONVERGECFD"):
        return "io"
    # everything else (Common*, Filters*, Imaging*, core IO, util) -> core
    return "core"

def needed(sopath):
    try:
        out = subprocess.check_output(["readelf","-d",sopath], stderr=subprocess.DEVNULL).decode()
    except Exception:
        return set()
    return set(re.findall(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]', out))

def main(pkgdir, outdir):
    pkg = Path(pkgdir)            # .../cvista
    files = list(pkg.rglob("*"))
    sos = [f for f in files if f.suffix == ".so" or ".so." in f.name]

    tier = {}   # path -> tier
    thirdparty = []  # in-package vtk 3rd-party libs to resolve via link graph
    libname = {}     # soname basename -> path  (for closure)

    for f in sos:
        b = f.name
        libname[b] = f
        if b.endswith(".abi3.so"):
            mod = b[:-len(".abi3.so")]
            tier[f] = module_tier(mod)
        elif b.startswith("libvtk") and SUFFIX in b:
            stem = b[len("lib"):].split(SUFFIX)[0]   # e.g. vtkIOExtra / vtkhdf5
            if stem in KIT_TIER:
                tier[f] = KIT_TIER[stem]
            else:
                thirdparty.append(f)                 # resolve below
        else:
            thirdparty.append(f)

    # link-graph closure: a 3rd-party lib's tier = lowest tier (core<rendering,io)
    # among libs that DT_NEEDED it. Compute reachable-from-tier sets.
    def closure(seed_paths):
        seen=set(); stack=list(seed_paths)
        while stack:
            p=stack.pop()
            if p in seen: continue
            seen.add(p)
            for n in needed(str(p)):
                if n in libname and libname[n] not in seen:
                    stack.append(libname[n])
        return seen
    core_reach = closure([p for p,t in tier.items() if t=="core"])
    rend_reach = closure([p for p,t in tier.items() if t=="rendering"])
    io_reach   = closure([p for p,t in tier.items() if t=="io"])
    for f in thirdparty:
        # core if needed by core, OR shared across BOTH rendering and io (a shared base
        # dep can only live in the common tier = core; rendering must not depend on io).
        if f in core_reach:                         tier[f]="core"
        elif (f in rend_reach) and (f in io_reach): tier[f]="core"
        elif f in rend_reach:                       tier[f]="rendering"
        elif f in io_reach:                         tier[f]="io"
        else:                                       tier[f]="core"   # unreferenced -> safe default

    # non-.so files: scaffolding -> core; .pyi dropped; everything else core
    for f in files:
        if f.is_dir() or f in tier: continue
        tier[f] = "core"

    # report
    from collections import Counter
    c = Counter(tier[f] for f in tier if (f.suffix==".so" or ".so." in f.name))
    print(json.dumps({"so_by_tier": dict(c),
                      "thirdparty_resolved": {Path(f).name: tier[f] for f in thirdparty}}, indent=2))

    if outdir:
        ALLOW={"core":{"core"},"rendering":{"rendering","core"},"io":{"io","core"}}
        tier_of_name={f.name:tier[f] for f in tier if (f.suffix==".so" or ".so." in f.name)}
        def undef(p):
            try: out=subprocess.check_output(["nm","-D","--undefined-only",str(p)],stderr=subprocess.DEVNULL).decode()
            except: return set()
            return set(re.findall(r'\b(_ZN\w+|\w+)\b', out))
        def defs(p):
            try: out=subprocess.check_output(["nm","-D","--defined-only",str(p)],stderr=subprocess.DEVNULL).decode()
            except: return set()
            return set(re.findall(r'\b(_ZN\w+|\w+)\b', out))
        stripped=0
        for t in ("core","rendering","io"):
            dst = Path(outdir)/t/"cvista"
            if dst.exists(): shutil.rmtree(dst)
            dst.mkdir(parents=True)
            for f in tier:
                if tier[f]!=t or f.is_dir(): continue
                rel = f.relative_to(pkg)
                (dst/rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, dst/rel)
            print(f"  {t}: {sum(1 for f in tier if tier[f]==t and not f.is_dir())} files -> {dst}")
        # Strip STALE cross-tier DT_NEEDED edges: a tier lib that DT_NEEDED a
        # disallowed-tier lib but shares ZERO symbols with it (the dep came from a
        # declared OPTIONAL_DEPENDS whose using-classes were gated out). Verified safe.
        for t in ("core","rendering","io"):
            for f in (Path(outdir)/t/"cvista").rglob("*"):
                if not (f.suffix==".so" or ".so." in f.name): continue
                u=undef(f)
                for n in needed(str(f)):
                    nt=tier_of_name.get(n)
                    if nt and nt not in ALLOW[t]:
                        # locate the depended lib to test symbol overlap
                        dep=libname.get(n)
                        overlap = (u & defs(dep)) if dep else {"?"}
                        if not overlap:
                            subprocess.check_call([PATCHELF,"--remove-needed",n,str(f)])
                            stripped+=1
                        else:
                            print(f"    REAL cross-tier dep kept: {f.name} -> {n} ({len(overlap)} syms)")
        print(f"  patchelf-stripped {stripped} stale cross-tier NEEDED edges")

        # ---- self-containment AUDIT (gate) -------------------------------------
        # After stripping stale edges, NO tier lib may DT_NEEDED a disallowed-tier
        # lib (core->only core; rendering->rendering+core; io->io+core). Any survivor
        # is a genuine partition break (a real symbol dependency crossing tiers) that
        # would fail to resolve when only that tier is installed.
        violations = []
        for t in ("core", "rendering", "io"):
            for f in (Path(outdir)/t/"cvista").rglob("*"):
                if not (f.suffix == ".so" or ".so." in f.name):
                    continue
                for n in needed(str(f)):
                    nt = tier_of_name.get(n)
                    if nt and nt not in ALLOW[t]:
                        violations.append(f"{t}: {f.name} -> {n} ({nt})")
        if violations:
            print("AUDIT FAILED — cross-tier dependencies survive:")
            for v in violations:
                print("  " + v)
            sys.exit(1)
        print("AUDIT PASSED — every tier lib resolves within {own tier} ∪ core")

if __name__=="__main__":
    pkgdir = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv)>2 else None
    main(pkgdir, outdir)
