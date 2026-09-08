"""Time sharp-edge splitting and the complete normals pipeline on folded sheets.

Use the same CPU affinity, VTK_SMP_MAX_THREADS, and build profile for both wheels.
Construction and warm-up are excluded. Each timed update reruns the filter.
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
from ops import make_sharp_edges_mesh
from vtkmodules.vtkCommonCore import vtkVersion
from vtkmodules.vtkFiltersCore import vtkPolyDataNormals, vtkSplitSharpEdgesPolyData


def measure(filter_, repeats):
    filter_.Update()
    times = []
    cpu_times = []
    for _ in range(repeats):
        filter_.Modified()
        start = time.perf_counter()
        cpu_start = time.process_time()
        filter_.Update()
        cpu_times.append(time.process_time() - cpu_start)
        times.append(time.perf_counter() - start)
    return statistics.median(times) * 1000, statistics.median(cpu_times) * 1000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=768)
    parser.add_argument("--repeats", type=int, default=9)
    args = parser.parse_args()
    results = []
    for width in (32, 64):
        for fixed, topology in (
            (False, "triangles"),
            (True, "triangles"),
            (False, "quads"),
            (True, "quads"),
        ):
            mesh = make_sharp_edges_mesh(np.float64, args.size, width, fixed, topology)
            mesh.BuildCells()
            mesh.BuildLinks()
            for angle in (30, 180):
                row = dict(
                    width=width,
                    fixed=fixed,
                    topology=topology,
                    angle=angle,
                    cells=mesh.GetNumberOfCells(),
                )
                for name, cls in (
                    ("split_ms", vtkSplitSharpEdgesPolyData),
                    ("normals_ms", vtkPolyDataNormals),
                ):
                    filter_ = cls()
                    filter_.SetInputData(mesh)
                    filter_.SetFeatureAngle(angle)
                    row[name], row[name.replace("_ms", "_cpu_ms")] = measure(
                        filter_, args.repeats
                    )
                    row["output_points"] = filter_.GetOutput().GetNumberOfPoints()
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
