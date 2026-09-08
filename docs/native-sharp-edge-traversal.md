# Native connectivity in sharp-edge traversal

`vtkSplitSharpEdgesPolyData` reads polygon connectivity through the existing
`cvistaCellConnectivity` view while finding smooth regions around each point.
This is a default-on, Bucket 1 optimization against stock VTK 9.7.0.

Surface-refresh workflows commonly extract a surface, retain int32 connectivity,
and compute normals with vertex splitting enabled. The normals filter invokes
this traversal after computing cell normals. Previously, every visited face
called `vtkPolyData::GetCellPoints`, widening its int32 connectivity into a
thread-local scratch list. The same face can be fetched from several vertices.
Native reads remove these repeated copies and accessor dispatches.

The change also consolidates the duplicated code that locates the two vertices
adjacent to a point in a face. It reuses an existing connectivity reader and adds
no public API, dependency, tuning option, or persistent cache.

## Identity argument

The native view is used only when all input cells are polygons and their storage
is one of the supported explicit/fixed-size int32/int64 layouts. Global cell ids
then equal local polygon ids. Deleted-cell tags still use the general accessor,
as do mixed cell types and generic storage. Input connectivity remains read-only
throughout marking; the view is no longer read when the output's connectivity is
copied and edited during reduction.

Reading an int32 point id and promoting its value to `vtkIdType` yields exactly
the value in the old widened scratch list. Both access paths share the same
adjacent-vertex lookup, including the original first/last-vertex special cases.
The seed edge order and subsequent choice of the other edge are unchanged.
Thus the same neighbors are visited in the same order, producing the same region
numbers, duplicated points, connectivity replacements, and attributes.

No normal computation, floating-point comparison, feature-angle calculation,
threading policy, reduction order, or output-precision choice changes.

## Validation and reproduction

The `sharp_edges_storage` bit-exact operation covers float32/float64 points,
explicit and fixed-size int32/int64 triangles and quads, mixed polygon sizes,
non-manifold edges, repeated vertices, short polygons, generic connectivity,
mixed cell types, deleted cells, and editable meshes. It captures inputs,
direct splitting at 0/30/180 degrees, and the full normals pipeline. It asserts
that input arrays remain unchanged and that both splitting and no-split cases
actually occur. Floating arrays are also compared as integer bit patterns,
including signed zeros. The operation participates in the 1/4/8-thread gate.

Build using the project's manylinux cibuildwheel flow. Use matching build
profiles for the baseline and candidate wheels. The benchmark excludes mesh
construction and warm-up, and forces each measured update to execute:

```bash
VTK_SMP_MAX_THREADS=1 taskset -c 8 /path/to/baseline/bin/python tools/bench-sharp-edges.py --size 768 --repeats 9
VTK_SMP_MAX_THREADS=1 taskset -c 8 /path/to/candidate/bin/python tools/bench-sharp-edges.py --size 768 --repeats 9
```

Choose an available CPU on the machine being measured. These synthetic timings
measure filter execution, not end-to-end application latency.

The candidate was built with `CVISTA_LTO=0 CVISTA_GATE_O2=1
CVISTA_SOURCE_UNITY=0`, matching the Linux PR gate. Local validation passed
**885 bit-exact tests**, including the 1/4/8-thread checks. At the pinned PyVista
commit `9e85d7045eb7455ac39437cdfea2ef08893b85b8`, the full core suite passed
10,025 tests. Its only failure was `tests/test_cli.py::test_validate_color`, also
present in the stock VTK run (10,228 passes). JUnit failure-node comparison found
zero new core failures. The full plotting suite passed 2,656 tests on both
backends, with only the shared `test_offscreen_probe_follows_qt_platform`
failure caused by this host's EGL fallback. The stock runs used the same pinned
PyVista checkout and were recorded during #291 validation; neither the stock
wheel nor PyVista changed. JUnit comparison found zero new plotting failures.

All 13 renderexact scenes matched stock in RGBA and depth, using the same NVIDIA
GTX 1080 EGL driver (580.173.02).

## Local measurements

Baseline: the #291 wheel, whose C++ code is now on main at
`482bb2f8e493cdd75ff87bab5d513ead2fd4d30b`. Both wheels use the PR build profile
above. Intel i9-14900KF, CPU affinity 8 (a performance core), one VTK thread,
CPython 3.13, NumPy 2.4.6, float64 points, size 768: 1,176,578 triangles or
588,289 quads. Feature angle 30 degrees. Each run takes the median of nine
updates after warm-up; the table averages two run medians per wheel, measured
in baseline/candidate/candidate/baseline order.

| Id width | Offsets | Faces | Splitting, baseline → candidate (ms) | Full normals update, baseline → candidate (ms) |
| --- | --- | --- | --- | --- |
| 32 | explicit | triangles | 131.57 → 125.31 (1.05×) | 307.59 → 302.02 (1.02×) |
| 32 | fixed-size | triangles | 138.17 → 111.10 (1.24×) | 308.11 → 280.02 (1.10×) |
| 32 | explicit | quads | 89.12 → 86.15 (1.03×) | 177.84 → 176.94 (1.01×) |
| 32 | fixed-size | quads | 107.47 → 88.59 (1.21×) | 206.72 → 188.75 (1.10×) |
| 64 | explicit | triangles | 111.66 → 108.79 (1.03×) | 282.24 → 280.89 (1.00×) |
| 64 | fixed-size | triangles | 125.87 → 111.70 (1.13×) | 286.69 → 272.93 (1.05×) |
| 64 | explicit | quads | 85.98 → 86.37 (1.00×) | 173.91 → 174.81 (0.99×) |
| 64 | fixed-size | quads | 96.11 → 88.12 (1.09×) | 190.98 → 183.24 (1.04×) |

The strongest improvement is on fixed-size int32 connectivity: 1.21–1.24× for
splitting and about 1.10× for the complete normals update. Explicit int32
connectivity improves less; explicit int64 connectivity is largely unchanged.
Changes around 1–2% are within this host's observed timing variation. Repeated
candidate splitting medians differed by less than 2%; CPU-time ratios tracked
wall-time ratios. The benchmark also records the 180-degree no-split control
and process CPU time. Direct splitting starts with cells/links already built;
the full normals update includes its internal orientation and splitting stages.

Candidate manylinux wheel SHA-256:
`a64530f43cb50ccb5fbeb35e6e588df68a97774e275b1945cbf87dbbf1b3e60b`.
