# Native connectivity in polygon orientation

`vtkOrientPolyData` now reads polygon ids through the existing
`cvistaCellConnectivity` view. Its native and fallback accessors feed one shared
breadth-first traversal. This is a default-on, Bucket 1 optimization against
stock VTK 9.7.0, following the cell-normal and sharp-edge work in #291/#292.

Surface-normal updates with consistency enabled run this stage before computing
normals and splitting sharp vertices. Previously, each visited face and each
unvisited neighbor went through `GetCellPoints`, repeatedly widening int32 ids
into scratch lists. The new path reads those values directly. Connectivity and
typed edge-neighbor readers are resolved once per update, so setup is shared
across disconnected components as well as across faces in one component.

The change reuses existing readers and adds no public API, persistent cache,
dependency, alternative orientation rule, or threading policy.

In Femorph, FEM and rotor exterior construction call `compute_normals` with
consistency enabled while preparing surfaces with sharp-edge splitting. These
paths benefit directly, including when rebuilding exterior meshes for the GUI.
GUI refreshes that explicitly disable consistency or use Femorph's custom
point-normal helper do not run this stage. The measurements below characterize
the filters, not whole-application latency.

## Identity argument

The native view is enabled only for polygon-only input with explicit/fixed-size
int32/int64 storage. Output cell tags were rebuilt from the copied polygons, so
global cell ids equal local polygon ids. Mixed cell types and generic storage
use the existing general accessor. Deleted input tags retain stock's behavior:
the output rebuilds its tags from the polygon array.

The view reads **output** connectivity, which is modified as traversal proceeds.
`ReverseCell` reverses values in place without reallocating native buffers, so
later visits see all earlier flips. A current wave cell is already marked
visited; the neighbor loop only reverses unvisited cells. Therefore its own
connectivity cannot change during that loop, and a live view returns the same
ids as the old scratch snapshot. Integer widening preserves each id's value.

Input cell links remain valid when winding changes: reversal changes the order
of ids, not point-to-cell membership. Resolving their typed reader once per
update thus gives the same edge-neighbor sequence as resolving it per component.

The shared traversal preserves wave order, edge order, neighbor order, visited
flags, reversal decisions, and reversal counts. Auto-orientation's priority
queue, tie handling, seed selection, and floating-point calculations are
untouched. So are normal computation, output attributes, abort/progress points,
and the separate opt-in fast orientation path.

## Validation and reproduction

The `orient_storage` operation compares exact connectivity, points, attributes,
and full normals-pipeline outputs against stock. It covers winding repairs,
flips, auto-orientation, non-manifold traversal, float32/float64 points, native
int32/int64 layouts, triangles/quads/mixed polygons, generic storage, mixed cell
types, deleted cells, editable meshes, disconnected triangles, and closed
components with equal minimum x coordinates. Floating arrays are also compared
as integer bit patterns. It checks input polygon preservation and captures
stock's shared-line reversal side effect on mixed inputs. These are strict
comparisons, with no orientation/order relaxation. The operation is also in the
1/4/8-thread determinism gate. The harness now compares each completed,
immutable thread-count dump pair once per module and shares that result among
filter assertions; it runs the same filters and retains every assertion.

Build both wheels through the project's manylinux cibuildwheel flow with the
same profile. Benchmark example (choose an available CPU on the host):

```bash
VTK_SMP_MAX_THREADS=1 CVISTA_FAST=0 taskset -c 8 /path/to/baseline/bin/python tools/bench-orientation.py --size 512 --repeats 7
VTK_SMP_MAX_THREADS=1 CVISTA_FAST=0 taskset -c 8 /path/to/candidate/bin/python tools/bench-orientation.py --size 512 --repeats 7
```

The benchmark records median wall and process CPU times for consistent sheets,
scrambled winding, quads, and disconnected triangles. Construction, initial
cell/link building, and warm-up are excluded. Each timed `Modified()`/`Update()`
reruns the filter. The full normals measurement also includes the internal
orientation and splitting stages; these are filter timings, not application
latency measurements.

## Local measurements

Baseline: #292 at `239ed03ffd3cc0a370f1f4ac4ac65c1719a7a40c`. Both wheels use
`CVISTA_LTO=0 CVISTA_GATE_O2=1 CVISTA_SOURCE_UNITY=0`, matching the Linux PR
profile. Intel i9-14900KF, affinity pinned to performance-core CPU 8, one VTK
thread, CPython 3.13, NumPy 2.4.6, float64 points, size 512: 522,242 triangles
or 261,121 quads. Each run takes the median of seven updates after warm-up;
the table averages two run medians per wheel, measured in
baseline/candidate/candidate/baseline order.

| Id width | Offsets | Mesh | Orientation, baseline → candidate (ms) | Full normals speedup |
| --- | --- | --- | --- | --- |
| 32 | explicit | triangles, consistent | 34.08 → 27.70 (1.23×) | 1.06× |
| 32 | explicit | triangles, scrambled | 36.21 → 31.78 (1.14×) | 1.03× |
| 32 | explicit | quads, scrambled | 16.14 → 14.34 (1.13×) | 1.02× |
| 32 | explicit | isolated triangles | 8.96 → 7.86 (1.14×) | 1.03× |
| 32 | fixed-size | triangles, consistent | 39.52 → 27.08 (1.46×) | 1.09× |
| 32 | fixed-size | triangles, scrambled | 41.41 → 31.46 (1.32×) | 1.08× |
| 32 | fixed-size | quads, scrambled | 19.81 → 14.97 (1.32×) | 1.07× |
| 32 | fixed-size | isolated triangles | 11.72 → 7.93 (1.48×) | 1.06× |
| 64 | explicit | triangles, consistent | 32.88 → 27.35 (1.20×) | 1.04× |
| 64 | explicit | triangles, scrambled | 35.41 → 32.26 (1.10×) | 1.02× |
| 64 | explicit | quads, scrambled | 16.12 → 13.95 (1.16×) | 1.02× |
| 64 | explicit | isolated triangles | 9.15 → 7.48 (1.22×) | 1.02× |
| 64 | fixed-size | triangles, consistent | 36.33 → 27.23 (1.33×) | 1.08× |
| 64 | fixed-size | triangles, scrambled | 38.59 → 31.66 (1.22×) | 1.05× |
| 64 | fixed-size | quads, scrambled | 18.42 → 14.65 (1.26×) | 1.04× |
| 64 | fixed-size | isolated triangles | 10.40 → 7.54 (1.38×) | 1.04× |

Fixed-size int32 orientation improved 1.32–1.48×, including disconnected input;
full normals updates improved 1.06–1.09×. Explicit-storage inputs show smaller
whole-filter gains. These are single-machine measurements, and changes of a
few percent should be interpreted with the usual timing variability. Additional
four-thread runs (affinity 8,10,12,14) retained orientation gains, but whole-filter
timings varied more; the table reports the repeated single-thread measurements.

Candidate manylinux wheel SHA-256:
`f8dbda14c0639dff028ff72cfcb4853ac04a782f8202dbaa484d9bcaaf2691aa`.

## Validation results

The manylinux wheel passed **891 bit-exact tests**, including the 1/4/8-thread
checks. Full PyVista runs at pinned commit
`9e85d7045eb7455ac39437cdfea2ef08893b85b8` passed 10,025 core tests with cvista
and 10,228 with stock; plotting passed 2,656 tests on each backend. Both backends
had the same CLI color-formatting and offscreen-platform-probe failures.
JUnit failure-node comparison found **zero new failures**. Both test runners
prepend their own virtual environment to PATH so CLI/leak-test subprocesses use
the intended interpreter.

All **13 renderexact scenes** matched stock in RGBA and depth on the same
NVIDIA GTX 1080 EGL driver (580.173.02).
