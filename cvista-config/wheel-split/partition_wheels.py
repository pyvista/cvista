#!/usr/bin/env python3
"""partition_wheels.py — split one full cvista wheel into core/rendering/io tiers.

Input : the UNPACKED root of a built full cvista wheel (kits on, IOExtra kit split).
        That root holds the `cvista/` package AND — after the platform repair step —
        a vendored-runtime dir (auditwheel `cvista.libs/`, delocate `cvista/.dylibs/`,
        delvewheel `cvista.libs/`). BOTH are partitioned; a vendored lib rides the
        lowest tier whose kept libs load it (usually core, e.g. libgomp).
Output: three tier trees under <outdir>/{core,rendering,io}, each a wheel-root layout
        (cvista/ [+ vendored dir]) that shares the cvista/ import namespace and
        resolves cross-tier via the platform's $ORIGIN/@loader_path/dir-local search.

Classification (by shared-lib basename, platform-agnostic):
  * vtk<Module> python ext (vtkX.abi3.so / vtkX.pyd) -> MODULE_TIER (by module name)
  * libvtk<Kit>-cvista.{so,dylib,dll}                -> KIT_TIER (by kit basename)
  * any other shared lib (3rd-party, vendored runtime) -> link-graph closure:
        lowest tier whose kept libs load it; shared across rendering&io -> core;
        default core.
  * package scaffolding (__init__.py, util/, py.typed, numpy_*) -> core

Since the #167/#168 relocations made FiltersHybrid/IOGeometry unconditionally
rendering-free, the built tree has ZERO stale cross-tier link edges (measured:
`patchelf-stripped 0`), so NO load-command surgery is needed on any platform —
this is a pure classify + copy. The ELF stale-edge strip below is kept as a guarded
safety net (linux only) and is expected to be a no-op.
"""
import os, re, sys, shutil, subprocess, json, platform
from pathlib import Path

SUFFIX = "-cvista"  # VTK_CUSTOM_LIBRARY_SUFFIX
SHLIB_RE = re.compile(r"\.(so|dylib|pyd|dll)$|\.so\.\d")

# ---- kit lib basename (without lib/.so/suffix) -> tier --------------------
KIT_TIER = {
    "vtkCommon": "core", "vtkFilters": "core", "vtkImaging": "core",
    # The VTK::IO kit (IOCore/IOXML/IOXMLParser/IOLegacy/IOImage/IOGeometry/IOPLY/
    # IOCellGrid) rides the io tier: after the IO-out-of-core decoupling no core
    # module links any IO module, so the whole IO kit lib lives in io.
    "vtkIO": "io",
    "vtkRendering": "rendering", "vtkInteraction": "rendering",
    "vtkChartsCore": "rendering", "vtkViews": "rendering",
    "vtkIOExtra": "io",
    # tiered.cmake disables IOParallel/IOParallelXML/IOCGNSReader/IOInfovis, so the
    # VTK::Parallel kit holds ONLY core modules (FiltersParallel/Extraction/FlowPaths/
    # ParallelCore/ParallelDIY/ParallelDIY2) -> core tier.
    "vtkParallel": "core",
}

PATCHELF = shutil.which("patchelf") or "patchelf"  # from PATH (CI + Nix); no hardcoded store path


def module_tier(mod):
    m = mod
    if re.match(r'vtk(Rendering|Interaction|Charts|Views|Web|DomainsChemistry)', m):
        return "rendering"
    if m in ("vtkPythonContext2D",):
        return "rendering"
    if m == "vtkFiltersHybridRendering":       # the 3 view-classes split out of FiltersHybrid
        return "rendering"
    if m == "vtkImagingHybridIO":              # vtkSliceCubes split out of ImagingHybrid (needs IOImage)
        return "io"
    # Rendering-coupled IO modules: they DEPEND on RenderingCore (import/export scene
    # graph, chemistry). They stay in the rendering tier; since rendering may depend on
    # io (see ALLOW), any IO they pull resolves within rendering∪io∪core.
    # NOTE: vtkIOImport carries the glTF reader/texture relocated in #168.
    if m in ("vtkIOExport","vtkIOExportGL2PS","vtkIOImport","vtkIOMINC",
             "vtkIOChemistry","vtkInfovisCore","vtkInfovisLayout"):
        return "rendering"
    # The VTK::IO kit modules are now rendering-free and ride the io tier, together
    # with the standalone io-only readers/writers.
    if m in ("vtkIOCore","vtkIOXML","vtkIOXMLParser","vtkIOLegacy","vtkIOImage",
             "vtkIOGeometry","vtkIOPLY","vtkIOCellGrid",
             "vtkIOHDF","vtkIOExodus","vtkIOEnSight","vtkIOInfovis","vtkIONetCDF",
             "vtkIOVeraOut","vtkIOSegY","vtkIOFLUENTCFF","vtkIOCGNSReader",
             "vtkIOParallel","vtkIOParallelXML","vtkIOParallelExodus","vtkIOCONVERGECFD"):
        return "io"
    return "core"


def is_shlib(name):
    return bool(SHLIB_RE.search(name))


def is_python_ext(name):
    """A cvista python module extension: vtk<Module>.abi3.so / vtk<Module>.pyd (NOT libvtk*)."""
    return name.startswith("vtk") and not name.startswith("libvtk") and is_shlib(name)


def module_name(name):
    return name.split(".")[0]   # vtkRenderingCore.abi3.so / .pyd -> vtkRenderingCore


def needed(path):
    """DT_NEEDED / LC_LOAD_DYLIB / PE-import basenames — platform-detected by tool."""
    p = str(path)
    # ELF (linux)
    if shutil.which("readelf"):
        try:
            out = subprocess.check_output(["readelf", "-d", p], stderr=subprocess.DEVNULL).decode()
            if "Shared library:" in out or "(NEEDED)" in out:
                return set(re.findall(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]', out))
        except Exception:
            pass
    # Mach-O (macOS)
    if shutil.which("otool"):
        try:
            out = subprocess.check_output(["otool", "-L", p], stderr=subprocess.DEVNULL).decode()
            deps = re.findall(r'^\s+(\S+)\s+\(', out, re.M)
            return {os.path.basename(d) for d in deps if os.path.basename(d) != os.path.basename(p)}
        except Exception:
            pass
    # PE (windows) — pefile is pip-installable on the runner
    if p.endswith((".dll", ".pyd")):
        try:
            import pefile
            pe = pefile.PE(p, fast_load=True)
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
            return {e.dll.decode().lower() for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])}
        except Exception:
            pass
    return set()


def main(rootdir, outdir):
    root = Path(rootdir)                     # unpacked wheel root (cvista/ [+ vendored dir] [+ dist-info])
    pkg = root / "cvista"
    files = [f for f in root.rglob("*")
             if f.is_file()
             and not any(p.endswith(".dist-info") for p in f.relative_to(root).parts)]
    sos = [f for f in files if is_shlib(f.name)]

    # Authoritative module-name set, taken from the python extensions inside the
    # cvista/ package (vtkIOHDF.abi3.so / vtkIOHDF.pyd -> "vtkIOHDF"). A standalone
    # module lib is then tiered DETERMINISTICALLY via module_tier() rather than via
    # link-graph closure -- which is fragile on Windows/macOS where delvewheel /
    # delocate hash-mangle the built libs (vtkIOHDF-cvista-<hash>.dll) and move them
    # to a sibling vendor dir, so the .pyd -> lib NEEDED edge is easy to miss.
    module_names = {module_name(f.name) for f in sos
                    if is_python_ext(f.name) and pkg in f.parents}

    tier = {}            # path -> tier
    thirdparty = []      # libs resolved by link-graph closure
    libname = {}         # soname/basename -> path (for closure)

    for f in sos:
        b = f.name
        libname[b] = f
        # The -cvista suffix (VTK_CUSTOM_LIBRARY_SUFFIX) cleanly separates BUILT vtk
        # libs from python module extensions on every platform: libs carry it
        # (unix libvtkCommon-cvista.so / win vtkCommon-cvista.dll), module exts do
        # not (vtkCommonCore.abi3.so / vtkCommonCore.pyd). Vendored runtime libs
        # (libgomp, msvcp, delocate .dylibs) carry neither vtk-name nor -cvista.
        if SUFFIX in b:                                     # a built vtk shared lib
            stem = b[len("lib"):] if b.startswith("lib") else b
            stem = stem.split(SUFFIX)[0]                    # vtkCommon / vtkIOExtra / vtkIOHDF / vtkhdf5
            if stem in KIT_TIER:
                tier[f] = KIT_TIER[stem]                   # kit lib -> kit tier
            elif stem in module_names:
                tier[f] = module_tier(stem)                # standalone module lib -> module tier (deterministic)
            else:
                thirdparty.append(f)                       # vtk 3rd-party (vtkhdf5/png/...) -> closure
        elif b.startswith("vtk") and is_shlib(b) and pkg in f.parents:
            tier[f] = module_tier(module_name(b))          # python module ext -> module tier
        else:
            thirdparty.append(f)                           # vendored runtime (libgomp, ...) -> closure

    # link-graph closure: a 3rd-party lib's tier = lowest tier among libs that load it.
    def closure(seed_paths):
        seen = set(); stack = list(seed_paths)
        while stack:
            p = stack.pop()
            if p in seen:
                continue
            seen.add(p)
            for n in needed(str(p)):
                if n in libname and libname[n] not in seen:
                    stack.append(libname[n])
        return seen
    core_reach = closure([p for p, t in tier.items() if t == "core"])
    rend_reach = closure([p for p, t in tier.items() if t == "rendering"])
    io_reach   = closure([p for p, t in tier.items() if t == "io"])
    for f in thirdparty:
        if f in core_reach:                          tier[f] = "core"
        elif (f in rend_reach) and (f in io_reach):  tier[f] = "core"   # shared base -> common tier
        elif f in rend_reach:                        tier[f] = "rendering"
        elif f in io_reach:                          tier[f] = "io"
        else:                                        tier[f] = "core"   # unreferenced -> safe default

    # non-shlib files: cvista/ scaffolding -> core (dist-info already excluded)
    for f in files:
        if f in tier:
            continue
        tier[f] = "core"

    from collections import Counter
    c = Counter(tier[f] for f in tier if is_shlib(f.name))
    print(json.dumps({"so_by_tier": dict(c),
                      "thirdparty_resolved": {Path(f).name: tier[f] for f in thirdparty}}, indent=2))

    if not outdir:
        return

    # Tier dependency DAG: core (standalone) <- io (needs core) <- rendering (needs
    # io + core). Rendering may depend on io because scene import/export and molecule
    # rendering pull IO: e.g. vtkIOExport -> vtkDomainsChemistry -> vtkIOXMLParser, and
    # texture/image loading via vtkIOImage. Core stays pure (no io, no rendering).
    ALLOW = {"core": {"core"}, "rendering": {"rendering", "io", "core"}, "io": {"io", "core"}}
    tier_of_name = {f.name: tier[f] for f in tier if is_shlib(f.name)}

    for t in ("core", "rendering", "io"):
        dst = Path(outdir) / t
        if dst.exists():
            shutil.rmtree(dst)
        n = 0
        for f in tier:
            if tier[f] != t:
                continue
            rel = f.relative_to(root)                 # preserve cvista/... and vendored dir
            (dst / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, dst / rel)
            n += 1
        print(f"  {t}: {n} files -> {dst}")

    # ---- ELF stale-edge strip (linux only; expected NO-OP post-relocation) -----
    stripped = 0
    if platform.system() == "Linux" and shutil.which(PATCHELF):
        def syms(p, which):
            try:
                out = subprocess.check_output(["nm", "-D", which, str(p)], stderr=subprocess.DEVNULL).decode()
            except Exception:
                return set()
            return set(re.findall(r'\b(_ZN\w+|\w+)\b', out))
        for t in ("core", "rendering", "io"):
            for f in (Path(outdir) / t).rglob("*"):
                if not (f.is_file() and is_shlib(f.name)):
                    continue
                u = syms(f, "--undefined-only")
                for n in needed(str(f)):
                    nt = tier_of_name.get(n)
                    if nt and nt not in ALLOW[t]:
                        dep = libname.get(n)
                        overlap = (u & syms(dep, "--defined-only")) if dep else {"?"}
                        if not overlap:
                            subprocess.check_call([PATCHELF, "--remove-needed", n, str(f)])
                            stripped += 1
                        else:
                            print(f"    REAL cross-tier dep kept: {f.name} -> {n} ({len(overlap)} syms)")
        print(f"  patchelf-stripped {stripped} stale cross-tier NEEDED edges")

    # ---- self-containment AUDIT (gate, all platforms) --------------------------
    violations = []
    for t in ("core", "rendering", "io"):
        for f in (Path(outdir) / t).rglob("*"):
            if not (f.is_file() and is_shlib(f.name)):
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


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
