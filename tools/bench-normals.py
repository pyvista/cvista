"""Benchmark cell normals and complete surface-normal updates on synthetic meshes.

Run with the stock or cvista-shim interpreter used by tests/bitexact. For an
apples-to-apples comparison, use the same VTK_SMP_MAX_THREADS and build profile.
Mesh construction and the first warm-up are excluded from the timings.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import statistics
import sys
import time

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests" / "bitexact"))
from ops import make_normals_storage_mesh
from vtkmodules.vtkCommonCore import vtkVersion
from vtkmodules.vtkFiltersCore import vtkPolyDataNormals


def measure(fn, repeats):
    fn()
    times = []
    for _ in range(repeats):
        start = time.perf_counter()
        result = fn()
        times.append(time.perf_counter() - start)
        del result
    return statistics.median(times) * 1000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--repeats", type=int, default=9)
    args = parser.parse_args()
    results = []
    for dtype in (np.float32, np.float64):
        for width, fixed, mixed in (
            (32, False, False),
            (32, True, False),
            (32, False, True),
            (64, False, False),
            (64, False, True),
        ):
            mesh = make_normals_storage_mesh(dtype, args.size, width, fixed, mixed)
            mesh.BuildCells()
            row = dict(
                dtype=np.dtype(dtype).name,
                width=width,
                fixed=fixed,
                mixed=mixed,
                cells=mesh.GetNumberOfPolys(),
            )
            row["cell_ms"] = measure(
                lambda: vtkPolyDataNormals.GetCellNormals(mesh), args.repeats
            )
            for split in (False, True):
                normals = vtkPolyDataNormals()
                normals.SetInputData(mesh)
                normals.SetComputePointNormals(True)
                normals.SetComputeCellNormals(False)
                normals.SetSplitting(split)
                normals.SetConsistency(split)

                def update():
                    normals.Modified()  # force execution rather than a cached Update
                    normals.Update()

                row["split_ms" if split else "smooth_ms"] = measure(
                    update, args.repeats
                )
            results.append(row)
    print(
        json.dumps(
            dict(
                vtk=vtkVersion.GetVTKVersion(),
                threads=os.environ.get("VTK_SMP_MAX_THREADS", "default"),
                size=args.size,
                repeats=args.repeats,
                results=results,
            ),
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
