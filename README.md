# fvtk — fast VTK for PyVista

**fvtk** ("f" for *fast*) is a divergent fork of [VTK](https://gitlab.kitware.com/vtk/vtk)
maintained by the [PyVista](https://github.com/pyvista) organization. It is **not** a
mirror of upstream VTK and it is **not** affiliated with Kitware.

The project has two phases:

1. **Now — a trimmed, drop-in VTK.** fvtk contains only the modules PyVista imports
   (core + filters + IO + the full rendering stack) and their dependencies, built as a
   1:1 functional replacement for the stock `vtk` wheel. Smaller wheel, faster build,
   identical behavior on PyVista's own test suite.
2. **Next — swap-for-faster.** Individual VTK components are progressively replaced with
   faster implementations and dead code is stripped, so fvtk diverges from upstream over
   time. The trim is the baseline; the divergence is the point.

This is the result of the **build-trim campaign** — phase 1. The rest of this document is
a handoff guide for continuing development.

---

## Status

| | fvtk (trimmed) | stock `vtk` 9.6.2 |
|---|---|---|
| Fork point | VTK `v9.6.2` (`f49a1dbafa`) | 9.6.2 |
| Wheel size (stripped) | **49 MB** (47 MiB) | ~120 MB |
| Modules shipped | ~84 + vendored deps | ~160 |
| Compile units (`ninja` steps) | **~6,900** (wrappers further batched by unity) | ~9,120 |
| Source tree (tracked) | **~140 MB** | ~320 MB |
| PyVista parity gate | **green** (0 fvtk-introduced failures) | reference |

---

## What the trim does

Everything below is **API/ABI compatible** — the emitted C++ and Python wrappers are the
same code, just *fewer translation units* and *fewer shipped symbols*. Runtime behavior is
unchanged (verified against the PyVista suite); none of these levers trades runtime
performance, and the build keeps `-O3`.

### Build-size levers

1. **Module deny-list** — `VTK_BUILD_ALL_MODULES OFF`; only PyVista's measured module
   closure is enabled (`fvtk-config/_modules_minimal.cmake`). Source for ~136 never-built
   module directories was removed outright (320 MB → 218 MB).
2. **Lever A — `NOWRAP` (1,173 classes).** Classes PyVista never touches keep their C++
   but skip Python wrapper generation. A single hook in `CMake/vtkModule.cmake` demotes
   `FVTK_NOWRAP_CLASSES` (`fvtk-config/_nowrap_classes.cmake`) from `CLASSES` to
   `NOWRAP_CLASSES`. The drop list is closed under "referenced by a kept class's header" so
   it always builds.
3. **Lever B — `NOCOMPILE` (742 classes).** Classes outside PyVista's reachable closure are
   dropped from the build entirely (no compile, no wrapper, no hierarchy). Hook in
   `CMake/vtkModule.cmake` driven by `fvtk-config/_nocompile_classes.cmake`. The drop set is
   closed under C++ source references, transitive `::New()` factory bases, kept-subclass
   vtable/typeinfo (undefined-at-`dlopen`), and generated `vtk*ObjectFactory.cxx` override
   registrations.
4. **Source pruning** — the source files of the NOCOMPILE classes, plus the
   `Documentation/`, `Examples/`, `.gitlab/` trees and per-module `Testing/` test
   code/data, are removed from the tree (218 MB → ~140 MB). See
   [Gotchas](#gotchas--hard-won-traps) for which files must NOT be deleted.

Levers A + B take the build from ~9,120 to ~6,900 `ninja` steps (−24%) — near the practical
floor for class removal under PyVista parity (PyVista is a near-complete VTK frontend, so
most of what remains is genuinely reachable).

### Compile-time lever

5. **Wrapper unity (`FVTK_WRAP_UNITY`).** Each generated `*Python.cxx` re-parses the same
   `vtkPython*.h` stack (~40 % of its `-O3` cost). A hook in
   `CMake/vtkModuleWrapPython.cmake` batches them into chunked unity translation units
   (default 32 wrappers/TU) that `#include` the per-class files. The wrappers are
   byte-identical. **Measured ~48 % less wrapper-compile CPU at `-O3`**; isolated
   wrapper-phase wall −58 % @ `-j8`, −44 % @ `-j22`. Chunking keeps the phase CPU-bound, so
   the win holds at high core counts (drop the chunk size for bigger CI runners). Unlike
   C++-source unity, generated wrappers have no anonymous-namespace symbol collisions.

### Binary-size levers (compiled C++)

7. **SOA array-dispatch OFF** (`VTK_DISPATCH_SOA_ARRAYS` + `VTK_DISPATCH_SCALED_SOA_ARRAYS`
   = `OFF`, reverting to VTK's own default). VTK's `vtkArrayDispatch` instantiates a templated
   fast-path for each array *layout* × value type at every filter call site. With SOA +
   ScaledSOA on, the type list is `AOS + SOA + ScaledSOA` × ~14 types ≈ 42 instantiations per
   site (and `Dispatch2`/`Dispatch3` multiply it N²/N³); AOS-only is ~14. PyVista only ever
   constructs **AOS** arrays (`numpy_to_vtk` → `vtkFloatArray` etc.), never SOA — so the SOA
   fast-paths are dead weight. Off = ~3× less generated dispatch code in the big Filters/Common
   kits, no behavior change (an SOA array, if one ever appeared, still works via the virtual
   `vtkDataArray` fallback). **This is the single biggest binary lever.** (The SOA array
   *classes* themselves stay — they're woven into CommonCore's array system; only their
   dispatch fast-paths are dropped.)

8. **Link-time dead-code elimination** (`-ffunction-sections -fdata-sections` +
   `-Wl,--gc-sections`). Emits each function/datum in its own section and lets the linker drop
   the unreachable ones. Safe with VTK's `-fvisibility=hidden` (only exported symbols are GC
   roots; factory/virtual/wrapper paths all go through exports). Removes real code and stacks
   on top of strip.

Levers 7 + 8 together take the stripped wheel **65 MB → 49 MB (−24%)** with the PyVista suite
green (9,731 passed / 8 pre-existing env-fails / 0 introduced).

### Wheel-size lever

6. **Symbol strip (`FVTK_STRIP=1`).** `strip --strip-all` on every shipped `.so` removes
   the static symbol table (`.symtab`/`.strtab`, ~40 % of a Release lib — e.g. `libvtkCommon`
   178 → 107 MB) while keeping `.dynsym` (runtime-safe; matches stock manylinux wheels). The
   strip walks the wheel-staging tree, so all 142 shipped libs (incl. the kit libraries) are
   stripped, not just the SDK wrappers. **Measured: with levers 7+8, stripped wheel = 49 MB
   (47 MiB) — ~60 % smaller than the stock `vtk` wheel (~120 MB)** (strip alone, without 7+8,
   was 65 MB). The wheel is already maximally deflate-compressed; re-zipping does not shrink it.

> **Not used: auditwheel.** `auditwheel` is a *portability/tagging* tool, not a size tool —
> measured `repair` on our wheel was +620 bytes (it only retags `linux_x86_64` →
> `manylinux_2_39_x86_64`; VTK `dlopen`s GL so there's nothing to vendor). It will be needed
> at the CI/distribution stage to produce a PyPI-acceptable `manylinux` tag, but it does not
> shrink the wheel. LTO is a *runtime* lever (production profile `FAST=0`), not a size lever,
> and costs 2–3× build time.

---

## Building

The build self-execs into a `nix-shell` (`shell.nix`) that provides the GL/EGL/OSMesa/X11
stack, pins `cmake` 4.1.2, and uses Python 3.13 + `ccache`.

```bash
# Lightest parity wheel (default PROFILE=minimal, LTO off, stripped):
FVTK_STRIP=1 ./build-fvtk.sh

# Production wheel (LTO on, stripped):
FAST=0 FVTK_STRIP=1 ./build-fvtk.sh
```

Knobs (environment variables):

| var | default | meaning |
|---|---|---|
| `PROFILE` | `minimal` | `minimal` (PyVista closure) · `fast`/`linux` (all modules) |
| `FAST` | `1` | `0` enables LTO (production) |
| `FVTK_STRIP` | `0` | `1` strips shipped `.so` symbol tables |
| `BUILD` | `./build-fvtk` | build directory |
| `BUILD_JOBS` | `8` | parallel compile jobs |
| `USE_CCACHE` | `1` | compiler launcher via `ccache` |

The wheel lands in `<BUILD>/dist/*.whl`. Cold `-j8` build is ~29 min; warm (ccache)
rebuilds are minutes. **Always verify a deletion or config change in a *fresh* `BUILD` dir**
— a dirty incremental cache can hide a `configure`/generate break (see Gotchas).

---

## Repository layout

```
fvtk-config/            # all fvtk-specific build policy (the only "our code" in CMake terms)
  minimal.cmake         #   the default profile: deny-by-default + production knobs
  _modules_minimal.cmake#   the enabled-module closure (PyVista's measured set)
  _nowrap_classes.cmake #   Lever A drop list  (1,173 names) — skip Python wrapper, keep C++
  _nocompile_classes.cmake# Lever B drop list  (742 names)  — drop from build entirely
  fast.cmake / linux.cmake / macos.cmake / windows.cmake   # all-module profiles
build-fvtk.sh           # the build driver (nix re-exec, cmake configure, build, strip, wheel)
shell.nix               # GL/EGL/OSMesa/X11 + toolchain for the build
CMake/vtkModule.cmake            # hosts the NOWRAP + NOCOMPILE hooks (search "FVTK_")
CMake/vtkModuleWrapPython.cmake  # hosts the wrapper-unity hook (search "FVTK_WRAP_UNITY")
<Area>/<Module>/        # VTK source, trimmed (e.g. Common/Core, Filters/General, ...)
```

Everything outside `fvtk-config/`, `build-fvtk.sh`, `shell.nix`, and the three hook edits in
`CMake/` is upstream VTK source. The hooks are **inert until the lists are defined**, so the
tree still builds as stock VTK with a different `-C` cache file.

### Branches (on `pyvista/fvtk`, private)

| branch | purpose |
|---|---|
| `main` | the published trimmed fork — what `git clone` checks out |
| `feat/build-trim` | the build-trim campaign branch (this work) |
| `feat/fvtk-namespace` | optional `vtkmodules → fvtk` import rename (not merged) |
| `feat/ci` | early GitHub Actions wheel-matrix scaffolding (paused) |

---

## How the trim works (extending it)

All three levers are name-driven lists consumed by hooks; adding or restoring a class is a
one-line edit, no C++ changes.

- **To keep a class's Python wrapper** (undo a NOWRAP): delete its line from
  `_nowrap_classes.cmake`.
- **To compile a class again** (undo a NOCOMPILE): delete its line from
  `_nocompile_classes.cmake`.
- **To drop a new class**: add its name to a list. NOWRAP is zero-risk (C++ still compiles);
  NOCOMPILE needs closure analysis (the build is the oracle — `configure → build -k 0 →
  import-smoke`; undefined `::New()`/typeinfo symbols only surface at `dlopen`, not at link).
- **Wrapper-unity chunk size**: `FVTK_WRAP_UNITY_CHUNK` in `minimal.cmake` (must be a `CACHE`
  var to reach function scope). Smaller chunks parallelize better on big runners.

---

## Parity & validation

The gate is **PyVista's own test suite**, not a synthetic module-closure check: build the
fvtk wheel, install PyVista against it, run `tests/core` + `tests/plotting` off-screen
(EGL). Bar: **zero new failures versus stock `vtk` 9.6.2**.

Latest run: **9,731 passed / 8 failed**, where all 8 reproduce identically on a stock `vtk`
9.6.2 environment (missing optional `trame`, an image-cache cubemap test, a post-9.6.2
`VTKImplicitArray` feature, `test_tinypages` sphinx-env). **0 failures introduced by the
trim.**

How it's run:
- PyVista source: `/home/alex/source/pyvista` (already adapted to test against a custom
  wheel — **do not modify it**).
- A clean venv installs PyVista + test deps, force-installs the fvtk wheel, runs
  `pytest -n 8 --dist worksteal` off-screen.
- A parallel stock-`vtk` venv reproduces any suspected failure node-ID for apples-to-apples
  attribution — that's how the 8 env-fails were proven pre-existing.

Because source pruning does not change which classes compile (the NOCOMPILE list already
excluded them), a green configure + compile + import-smoke is sufficient proof a pruning
step is safe — the resulting wheel is functionally identical.

---

## Gotchas / hard-won traps

These cost real time during the campaign; read before extending.

1. **Not every `Testing/` dir is deletable.** `Testing/Core`, `Testing/DataModel`,
   `Testing/GenericBridge`, `Testing/IOSQL`, `Testing/Rendering`, `Testing/Serialization`
   carry a `vtk.module` and are referenced by VTK's top-level reject logic *even with
   `VTK_BUILD_TESTING OFF`* — deleting them gives "modules requested or required, but not
   found". Delete only per-module test *content* (`*/Testing/Cxx`, `Python`, `Data`), never a
   `Testing/*` directory that contains a `vtk.module`.
2. **NOCOMPILE only filters `CLASSES`.** A handful of classes are also listed in a module's
   explicit `SOURCES`/`TEMPLATES` (e.g. `vtkJoinTables.txx`, `vtkPolynomialSolversUnivariate`,
   `vtkExprTkFunctionParser`, `vtkThreadedCallbackQueue`) — those still compile, so their
   source files must NOT be deleted. Rule: never delete a file whose exact name appears in any
   `CMakeLists.txt`.
3. **Structurally-required disabled modules.** VTK's top-level `CMakeLists.txt`
   unconditionally references `VTK::mpi`, `VTK::catalyst`, `RenderingWebGPU`, Java,
   SerializationManager in its reject list; their definition dirs (`Utilities/MPI`,
   `Utilities/Catalyst`, `Rendering/WebGPU`, `Utilities/Java`, `Serialization/Manager`) must
   stay even though they never compile.
4. **WANT-silent-drop cascade.** Forcing a module to `NO` that is a transitive dependency of a
   loaded rendering module silently removes the whole rendering stack while configure still
   "succeeds". Classify removals by runtime-trace **and** dependency-closure; pyvista-loaded
   modules are pinned `YES` (loud-fail) in `_modules_minimal.cmake`.
5. **Prove deletions in a fresh build dir.** A dirty incremental cache can mask a generate
   break; the first "passing" build after a deletion may just be reusing stale state.
6. **nix shell python pinning.** `shell.nix` must provide Python 3.13; VTK derives the wheel
   ABI suffix from the first `python3` on `PATH`. `build-fvtk.sh` also installs a
   `.pyshim313` belt-and-suspenders.
7. **cmake pin.** nix `cmake` 4.1.2 must be first on `PATH`; a pip `cmake` 4.2.x regresses
   nested `try_compile`. `build-fvtk.sh` handles this.
8. **C++-source unity is hostile in VTK** (anonymous-namespace symbol collisions across
   `.cxx`). Only *wrapper* unity is safe. Don't retry C++ unity without an offline
   collision-offender list.

---

## Roadmap

- **CI** — wheel matrix mirroring PyVista's support set (Python 3.10–3.14 ×
  Linux-x86_64 / macOS-arm64 / Windows-x86_64). Build inside a `manylinux` image to lower the
  glibc floor, then `auditwheel repair` for the `manylinux` tag (portability, not size).
  Mine VTK's own `.gitlab/os-linux.yml` wheel jobs for the OSMesa/EGL handling — stock VTK
  wheels are not trivial cibuildwheel.
- **Namespace** — optional `vtkmodules → fvtk` import rename (`feat/fvtk-namespace`). Keeping
  `vtkmodules` preserves drop-in compatibility; renaming requires a PyVista-side dependency
  change.
- **Swap-for-faster** — replace hot VTK components with faster implementations. This is the
  divergence phase where fvtk earns the "f"; the trim is just the baseline.

---

## License

VTK is distributed under the OSI-approved BSD 3-clause License; see
[`Copyright.txt`](Copyright.txt). fvtk inherits that license. Upstream provenance (VTK
`v9.6.2`) is recorded in the root commit. For the original VTK project, see
[vtk.org](https://www.vtk.org/).
