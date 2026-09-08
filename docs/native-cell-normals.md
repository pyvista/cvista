# Native-connectivity cell normals

`vtkPolyDataNormals` now computes triangle cell normals directly from native
32-bit, 64-bit, and fixed-size connectivity. This is a default-on, Bucket 1
optimization against the current stock VTK 9.7.0 baseline.

Surface regeneration, smooth shading, sharp-edge splitting, and geometry
processing all use these cell normals. In particular, applications that keep
surface connectivity in int32 previously missed cvista's triangle shortcut:
`IsStorageShareable()` rejected their storage and each triangle went through
connectivity widening and point-array dispatch. One non-triangle also made the
old shortcut abandon completed work and recompute the entire polygon range.

The replacement dispatches connectivity once. Triangles read the native range;
other polygons use `vtkPolygon::ComputeNormal` individually. It removes the
shared `ok` flag, speculative writes followed by a full retry, and triangle
scratch-list allocation. It adds no public API, setting, dependency, cache, or
alternative normal-weighting rule.

## Identity argument

For AOS float/double triangles, the shortcut follows the triangle case of
`vtkPolygon.cxx`'s `NormalWorker`:

1. Subtract coordinates in the point-array value type, then promote to double.
2. Test the first edge's squared norm exactly as the stock worker does. A zero
   result leaves the normal zero, including when squaring a tiny edge underflows.
3. Compute the cross product in the same order and add it to the initially zero
   normal. That addition preserves stock's signed-zero behavior.
4. Apply the same `vtkMath::Normalize` and float conversion.

The previous shortcut omitted steps 2 and 3. The new regression inputs reproduce
both signed-zero and tiny-edge parity failures on the main-branch wheel.

Other polygon sizes retain stock's computation and vertex order. Non-AOS or
non-float/double point arrays retain the existing fallback. Cell-normal offsets,
vertex/line defaults, topology, attributes, point-normal accumulation order,
splitting, orientation, and the existing threading policy are unchanged.
Each worker writes only its own preallocated normal tuple; there is no shared
status write or cross-thread reduction.

## Validation and reproduction

The `normals_storage` and `normals_fallback` operations in `tests/bitexact/ops.py`
cover float32/float64 coordinates, int32/int64 connectivity with explicit and
implicit fixed-size offsets, mixed triangles/quads, short polygons, generic
connectivity, SOA/integral points, coincident/collinear triangles, signed zeros,
tiny edges, isolated vertices, lines, and attribute preservation. They capture
direct cell normals as well as the complete filter output. Floating arrays are
also captured as integer bit patterns so the comparison cannot equate `+0` and
`-0`.

The size-256 cases have over 130,000 polygons and are included in the existing
1/4/8-thread determinism gate, exceeding its 100,000-item threading threshold.
The existing smooth and sharp-edge-splitting cases remain in the stock parity
suite. The wheel is built through cibuildwheel's manylinux_2_28 flow with the
same `CVISTA_LTO=0 CVISTA_GATE_O2=1 CVISTA_SOURCE_UNITY=0` profile as the PR gate.

`tools/bench-normals.py` benchmarks both the cell-normal computation and complete
smooth/split-normal filter updates. Mesh creation and warm-up are excluded;
`Modified()` forces every measured filter update to execute. Run it with the
main-branch wheel and the candidate wheel in separate shim-enabled interpreters,
using identical CPU affinity and thread counts, for example:

```bash
VTK_SMP_MAX_THREADS=1 taskset -c 8 /path/to/main/bin/python tools/bench-normals.py --size 512 --repeats 9
VTK_SMP_MAX_THREADS=1 taskset -c 8 /path/to/candidate/bin/python tools/bench-normals.py --size 512 --repeats 9
```

Choose an available CPU on the machine being measured. Whole-filter performance
also includes orientation, link construction, and sharp-edge splitting; the
cell-normal speedup is not an end-to-end application speedup.

Local validation: **879 bit-exact tests passed**, including the native-storage
1/4/8-thread checks. PyVista at pinned commit
`9e85d7045eb7455ac39437cdfea2ef08893b85b8` passed its 179 PolyData tests on both
backends. The full core suite reported 10,025 passes with cvista and 10,228 with
stock, with the same sole failure (`tests/test_cli.py::test_validate_color`,
terminal-color/wrapping expectations). The full plotting suite passed 2,656
tests on each backend, with the same sole failure
(`test_offscreen_probe_follows_qt_platform`, this host's EGL fallback). Comparing
the JUnit failure node IDs found **zero new failures** in either suite.
The 13 rendering scenes were pixel- and depth-exact on the same NVIDIA GTX 1080
EGL driver (580.173.02).

## Local measurements

Baseline: main commit `ebb13fa2684bf724f1b25277d38ff48faaac531a`, using its
Linux PR-profile wheel from CI run `34152387240`. Candidate: the same
cibuildwheel profile, CPython 3.13, NumPy 2.4.6. Intel i9-14900KF, affinity
pinned to performance-core CPU 8, `VTK_SMP_MAX_THREADS=1`, size 512 (522,248
triangles, plus one quad for mixed inputs), median of nine measured executions
after warm-up:

| Coordinates | int32 connectivity | Cell normals, main → candidate | Smooth update speedup | Split update speedup |
| --- | --- | --- | --- | --- |
| float32 | explicit offsets | 15.52 → 3.94 ms (3.94×) | 1.62× | 2.20× |
| float32 | fixed-size offsets | 16.97 → 3.94 ms (4.31×) | 1.62× | 1.50× |
| float32 | triangles + quad | 15.63 → 3.92 ms (3.99×) | 1.73× | 1.53× |
| float64 | explicit offsets | 12.92 → 5.46 ms (2.37×) | 1.07× | 1.25× |
| float64 | fixed-size offsets | 12.54 → 3.60 ms (3.48×) | 1.39× | 1.59× |
| float64 | triangles + quad | 11.83 → 3.30 ms (3.59×) | 1.31× | 1.11× |

These are single-machine measurements, not universal speedup guarantees.
Complete split updates varied substantially between runs. The pre-existing
64-bit triangle shortcut already avoided widening: for float64, its cell-normal
time changed from 2.99 to 3.46 ms and smooth updates from 21.80 to 22.86 ms in
this run. The replacement also restores the stock edge-case arithmetic on that
path. Mixed float64/int64 cell normals improved from 13.38 to 3.46 ms because
the trailing quad no longer forces a retry of every triangle.
