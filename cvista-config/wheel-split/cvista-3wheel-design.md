# cvista 3-wheel split — design & partition manifest

## Goal
Ship three stackable PyPI wheels that share the `cvista/` import namespace:
- **cvista** (core): VTK Common + Filters + Imaging + native-format IO. Rendering-clean. Tiny, fast import. Offline processing.
- **cvista-rendering**: the rendering tier (OpenGL2, FreeType, Charts, Views, Interaction) + rendering-coupled IO. `Requires-Dist: cvista==<ver>`.
- **cvista-io**: heavy / third-party data IO (HDF, Exodus, CGNS, EnSight, NetCDF, SegY, FLUENT, …) + vendored hdf5/netcdf/cgns/exodus. `Requires-Dist: cvista==<ver>`.

## Why a single build can partition (after the IOExtra kit split)
All three wheels install files into the same `cvista/` directory; the shipped libs carry `RUNPATH=$ORIGIN`, so a lib in cvista-rendering resolves `libvtkCommon-cvista.so` from the core wheel sitting in the same dir. SONAMEs are identical because it is ONE build.

The only blocker was kit consolidation: `libvtkIO-cvista.so` (kit `VTK::IO`) bundled core-native IO **and** heavy IO (Exodus/FLUENTCFF/SegY) in one binary, so the core wheel could not take `libvtkIO` without dragging the heavy formats. **Keystone fix (`feat/io-extra-kit`):** retarget Exodus/FLUENTCFF/SegY to a new `VTK::IOExtra` kit → `libvtkIOExtra-cvista.so`. Now `libvtkIO` is core-clean and partition is exact.

## Tier assignment

### Kit libs (libvtk<Kit>-cvista.so)
| kit lib | tier |
|---|---|
| libvtkCommon | core |
| libvtkFilters | core |
| libvtkImaging | core |
| libvtkIO (core IO: XML/Legacy/PLY/Image/Core/XMLParser/CellGrid) | core |
| libvtkParallel (IOParallel/ParallelXML/CGNS — DEPENDS IOGeometry→Rendering) | io* |
| libvtkRendering | rendering |
| libvtkInteraction | rendering |
| libvtkChartsCore | rendering |
| libvtkViews | rendering |
| libvtkIOExtra (NEW: Exodus/FLUENTCFF/SegY) | io |

### Standalone module libs (kits-off or non-kitted modules) → tier by module
- core: (none beyond kits)
- rendering: RenderingOpenGL2/FreeType/ContextOpenGL2/GL2PSOpenGL2/Annotation/Label/Matplotlib/UI/Volume*/VtkJS/SceneGraph, IOExport, IOExportGL2PS, IOImport, IOMINC, IOChemistry, IOGeometry, DomainsChemistry
- io: IOHDF, IOEnSight, IOInfovis, IONetCDF, IOVeraOut, IOParallelExodus, InfovisCore (DEPENDS RenderingFreeType → actually rendering)

### Third-party vendored libs → tier (shared deps go to the lowest tier that needs them)
- core: zlib, expat, png, jpeg, tiff, pugixml, lz4, lzma, doubleconversion, fmt, loguru, utf8, token, scn, kissfft, vtksys, metaio, theora? (no→io), libxml2? (used by core IOXMLParser? check), kwiml, verdict, sqlite (used by core IO? — check)
- rendering: freetype, glad, gl2ps
- io: hdf5, hdf5_hl, netcdf, cgns, exodusII, libxml2 (if only used by exodus/cgns), theora, ogg

> NOTE: third-party tier assignment must be derived from `readelf -d` DT_NEEDED of each tier's
> kit/module libs against the actual built tree, NOT guessed. A 3rd-party lib goes to CORE if any
> core lib needs it; else rendering/io by its sole consumer. Script computes this from the link graph.

## Packaging mechanism
1. One full build (all modules, kits on, + IOExtra kit split).
2. `partition_wheels.py`: walk `cvista/`, classify every file via (a) module→tier table for `vtk*.abi3.so` + their `.py`, (b) kit→tier table for `libvtk*Kit*.so`, (c) link-graph closure for 3rd-party `.so`. Core also gets the package scaffolding (`__init__.py`, `vtkmodules` compat shims, `util/`, `numpy_*`, `py.typed`).
3. Emit 3 wheel trees + 3 generated setup.py / PKG-INFO with `Requires-Dist` back-edges to `cvista==<ver>`; `wheel pack` each.
4. Validate: in a clean venv, `pip install cvista-core.whl` → offline pipeline works, rendering absent. `pip install cvista-rendering.whl` on top → plotting works. `pip install cvista-io.whl` on top → HDF/Exodus read works.

## CI (trusted publishing for 3 packages)
- Three PyPI projects (cvista, cvista-rendering, cvista-io) each with a trusted-publisher entry pointing at the repo's release workflow.
- Release workflow builds once per (platform, pyver), runs partition_wheels.py, uploads all three artifacts. Pin coupling: cvista-rendering/io `Requires-Dist: cvista==<exact build ver>` so a user never mixes mismatched ABIs.

## DECISION (from the self-containment audit): separate per-tier builds, NOT partition-of-one-build

Partitioning a single FULL build does NOT yield a self-contained core. The audit
(`partition_wheels.py` + DT_NEEDED check) found these kit libs link ACROSS tiers in a
full build, so no file-level cut is clean:

- `libvtkFiltersHybrid → libvtkRendering`: a full build compiles the 3 view-dependent
  classes (PR #165's conditional only drops them when RenderingCore is ABSENT). A clean
  single-build partition would need those 3 classes in a SEPARATE rendering-tier module,
  not just conditionally compiled.
- `libvtkIO → libvtkRendering`: via IOGeometry's vtkOBJImporter → RenderingCore. IOGeometry
  must leave the core IO kit (own decouple, like FiltersHybrid) OR ride the rendering tier.
- `VTK::Parallel` kit mixes ParallelCore/ParallelDIY2 (CORE — FiltersParallel/Extraction/
  FlowPaths depend on them) with IOParallel/IOParallelXML/IOCGNSReader (io). Needs the same
  kit split as IO→IOExtra: ParallelCore→core kit, the IO-parallel readers→an io kit.
- `netcdf` is needed by Exodus(io) AND IOMINC(rendering) → shared → must be CORE.
- `IOInfovis(io) → InfovisCore(rendering)` and `InfovisCore → RenderingFreeType` → IOInfovis
  is effectively rendering-tier.

CONCLUSION: build each tier SEPARATELY (each tier enables only its modules + core as deps),
so each tier's kit libs contain only that tier's modules and link only downward. The
standalone `core.cmake` build is ALREADY a self-contained, working core (validated). The
IOExtra kit split (PR #166) + FiltersHybrid decouple (PR #165) make the per-tier builds
clean. `partition_wheels.py` + the audit remain useful as the tier-manifest + a CI gate
(every shipped tier wheel must pass the DT_NEEDED self-containment audit).

NEXT per-tier build configs needed: `rendering.cmake` (core deps + rendering modules,
package only rendering-tier files) and `io.cmake` (core deps + IOExtra/HDF/EnSight/NetCDF/
CGNS + vendored hdf5/netcdf/cgns/exodus, package only io-tier files). Each packages ONLY
its distinctive files; core kit libs come from the core wheel via $ORIGIN.

## SINGLE-BUILD realization (per maintainer choice: single build + more decoupling)

A `cvista-config/tiered.cmake` builds the union of all tiers, made tier-pure by a unified
`CVISTA_CORE_CLEAN` flag + disabling a few niche cross-cutting modules:

- `CVISTA_CORE_CLEAN=ON` excludes the rendering-coupled classes from core-tier modules even
  when RenderingCore is present (gated in each module's CMakeLists via
  `if (TARGET VTK::RenderingCore AND NOT CVISTA_CORE_CLEAN)`):
  - FiltersHybrid: vtkAdaptiveDataSetSurfaceFilter / vtkPolyDataSilhouette / vtkRenderLargeImage
  - IOGeometry: vtkGLTFReader / vtkGLTFTexture (the ONLY 2 IOGeometry classes that include
    vtkTexture.h — verified; RenderingCore made OPTIONAL_DEPENDS). OBJ/STL/PLY/FLUENT readers
    stay core-clean.
  These niche classes ship ONLY in the monolithic full cvista wheel.
- Disabled in tiered build (full wheel only): IOParallel (real RenderingOpenGL2/Parallel dep),
  IOCGNSReader, IOParallelXML, IOInfovis (→InfovisCore→RenderingFreeType) — they mix the core
  VTK::Parallel kit with io readers / pull rendering. Disabling keeps VTK::Parallel core-pure.
- IOExtra kit (PR #166) carries Exodus/FLUENTCFF/SegY so libvtkIO is core-clean.

Pipeline: build tiered.cmake once → `partition_wheels.py` (module/kit→tier + 3rd-party link-graph
closure) → DT_NEEDED self-containment AUDIT (gate) → `pack_tier_wheels.py` (3 wheels,
Requires-Dist back-edges) → `validate_stacking.sh` (core alone offline; +io heavy readers; +rendering).

CAUTION learned: removing a "stale" RenderingCore dep can break the build if a class #includes a
RenderingCore header even without emitting an undefined symbol at the nm level (vtkGLTFReader →
vtkTexture.h). Always grep sources for rendering #includes, not just nm the objects.

## Open sub-items
- IOGeometry/IOParallel/CGNS sit on a Rendering-coupled chain (IOGeometry→RenderingCore via vtkOBJImporter). For a clean io tier that depends only on core, IOGeometry needs the same RenderingCore-optional decouple as FiltersHybrid (PR #165 pattern), OR it rides in the rendering tier. Until then: IOParallel/CGNS → rendering tier or excluded.
- InfovisCore DEPENDS RenderingFreeType → rendering tier (already noted in core.cmake).
