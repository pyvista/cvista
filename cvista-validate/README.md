# cvista-validate — SMP parallel-vs-serial parity

cvista sets the library-wide `vtkSMPTools` backend to **STDThread**
(`cvista-config/minimal.cmake`), so every `vtkSMPTools::For` loop runs
multithreaded out of the box. This standalone C++ program proves that this does
**not** change any algorithm's output: for each filter it computes a serial
reference and re-runs it threaded, asserting the result is **byte-exact**.

It directly answers the concern that parallelizing a filter can make it unstable
(differ from serial, or vary run-to-run with thread scheduling).

## What it checks

For every registered algorithm (`smpParityCases.cxx`):

| axis | serial reference | parallel run |
|---|---|---|
| backend | `SetBackend("Sequential")` | `SetBackend("STDThread")` |
| threads | `Initialize(1)` | `Initialize(T)`, `T ∈ {2, 4, 8, oversubscribed}` |
| opt-in floor | `CVISTA_SMP_DEFAULT=0` (disables the 4-thread `RunSafeFilterParallel` default so the reference is a *true* serial floor) | default |
| repeats | 1 | N per thread count (default 4) — catches scheduling-dependent nondeterminism |

Outputs are compared **bit-for-bit** (`smpParityCompare.cxx`): point
coordinates, every cell array (offsets + connectivity), cell types, and all
point/cell data arrays — each value via `memcmp` so `NaN==NaN` and `+0.0!=-0.0`
behave as an on-disk comparison would. The process exits nonzero if any
algorithm diverges.

### Order-relaxed filters

A few filters (parallel cut/contour extraction — `vtkPlaneCutter`, `vtkCutter`,
`vtkContour3DLinearGrid`) emit the *same* geometry in a **thread-dependent
order**, so their threaded output is not byte-exact with serial. These are
tagged `orderRelaxed` and validated with a weaker-but-still-strict pair:

1. **run-to-run determinism** — repeated parallel runs at a fixed thread count
   must be byte-identical to each other (this is the real *instability* check; it
   holds even for order-relaxed filters), and
2. **same geometry set** — the parallel output must equal serial as an
   order-insensitive multiset of points (with point data) and cells (with cell
   data): `CompareGeometrySet` canonicalizes point order by sorting on
   coordinates+data and rewrites cell connectivity through that ranking.

So an order-relaxed filter passes only if it is deterministic *and* produces the
identical geometry — just possibly renumbered. A genuine nondeterminism or a
changed point/cell set still fails the gate.

Coverage spans every SMP risk class — per-element (`vtkWarpVector`,
`vtkElevationFilter`, `vtkTransformFilter`, `vtkThreshold`, …), reduction
(`vtkCellDataToPointData`, `vtkGradientFilter`, `vtkPolyDataNormals`, …),
isosurface/cut/clip (`vtkContourFilter`, `vtkFlyingEdges3D`, `vtkPlaneCutter`,
`vtkTableBasedClipDataSet`, …), and point-merge/hashing (`vtkGeometryFilter`,
`vtkStaticCleanUnstructuredGrid`, `vtkExtractEdges`, `vtkAppendPolyData`, …) —
across image, polydata, and unstructured-grid inputs.

## Build & run

cvista forces `VTK_BUILD_TESTING OFF`, so this is built **out of tree** against
an installed cvista (like the SDK examples), not via the in-tree test harness:

```bash
# 1. build+install cvista (produces the find_package(VTK) tree)
ci/build-sdk.sh                      # installs to ./sdk-install

# 2. build the validator against it and run
INSTALL_PREFIX=./sdk-install ci/run-smp-parity.sh
```

Or by hand against any cvista install / build tree:

```bash
cmake -S cvista-validate -B build-parity -DVTK_DIR=<prefix>/lib/cmake/vtk-9.6
cmake --build build-parity
./build-parity/cvista_smp_parity 8   # optional arg = repeats per thread count
```

CI runs this on Linux (GCC), macOS (AppleClang), and Windows (MSVC) via
`.github/workflows/smp-parity.yml`.

## Adding a filter

Append a `Case` in `smpParityCases.cxx` with a lambda that wires the filter onto
one of the shared inputs. If it needs a module not already linked, add the
component to `find_package(VTK … COMPONENTS)` in `CMakeLists.txt`.
