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

When a geometry/table divergence is reported it is annotated with a **magnitude**
— the symmetric max nearest-point coordinate deviation (via
`vtkStaticPointLocator`), or for tables the max gap between the two sorted value
multisets — so floating-point noise (`~1e-13`, benign reduction-order jitter) is
distinguishable at a glance from a macroscopic, genuinely-different result.

### Serial-vs-serial pre-check

Before any parallel comparison, each filter's serial run is done **twice** and
the two are compared byte-exact. A single-threaded filter must be deterministic;
if it isn't, its output is nondeterministic *regardless* of threading and a
parallel-vs-serial comparison against a moving reference is meaningless. Such a
filter is reported **SERIAL-UNSTABLE** (a real defect, but orthogonal to the
threading question) and is not charged against the gate. This auto-classifies
inherently-nondeterministic filters instead of the harness having to hard-code
them.

### Not-gated exceptions (reported with evidence)

A few of the ~65 filters are **not** byte-exact. The validator still runs them,
prints the divergence + magnitude, and classifies each — but does not count them
as gate failures (the gate protects the deterministic majority). The observed
exceptions fall into three kinds:

- **Nondeterministic even single-threaded (SERIAL-UNSTABLE) — not a threading
  question.**
  - `vtkLengthDistribution` samples cells and vertex pairs from a
    `vtkReservoirSampler` seeded from `std::random_device` on *every call*, so its
    output is random even serially (it should take a fixed seed to be testable).
  - `vtkSurfaceNets3D` produces byte-identical **geometry** (points +
    connectivity), but its `BoundaryLabels` cell-data is an uninitialized/wild
    read — garbage (`~9e8`, far outside the 0..3 label range) that changes every
    run, single-threaded included. A genuine bug, just not a threading one.
- **Genuine threading bug — tracked, pending fix.** `vtkDiscreteFlyingEdgesClipper2D`
  is serial-stable but shares one `vtkLabelMapLookup` across all threads, whose
  cache (`CachedValue`/`CachedOutValue`) is written unsynchronized — a data race
  that yields garbage point coordinates under threads (measured max coord dev
  `~3.4e37` on Linux vs `~1.2e10` on macOS: a platform-varying uninitialized
  read). The fix mirrors `vtkSurfaceNets3D`'s geometry path, which uses a
  per-thread `vtkSMPThreadLocal` lookup. Until fixed, threading it is unsafe.

This separation is the whole point of the validator: it distinguishes "parallel
is fine" (the large majority, byte-exact), "parallel reorders but is otherwise
identical" (order-relaxed), "unstable regardless of threading" (SERIAL-UNSTABLE),
and "parallel is genuinely broken here" (the one real race) — with evidence for
each.

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
