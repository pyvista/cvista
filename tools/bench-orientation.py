"""Benchmark orientation and full normals on consistent/scrambled surface meshes.

Run matching baseline/candidate wheels with identical affinity and thread count.
Construction, cell/link building, and warm-up are excluded from the timings.
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
from ops import make_orientation_mesh
from vtkmodules.vtkCommonCore import vtkVersion
from vtkmodules.vtkFiltersCore import vtkOrientPolyData, vtkPolyDataNormals


def measure(filter_, repeats):
    filter_.Update()
    wall, cpu = [], []
    for _ in range(repeats):
        filter_.Modified()
        start, cpu_start = time.perf_counter(), time.process_time()
        filter_.Update()
        cpu.append(time.process_time() - cpu_start)
        wall.append(time.perf_counter() - start)
    return statistics.median(wall) * 1000, statistics.median(cpu) * 1000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    results = []
    for width in (32, 64):
        for fixed in (False, True):
            for topology, scrambled, disconnected in (
                ("triangles", False, False),
                ("triangles", True, False),
                ("quads", True, False),
                ("triangles", False, True),
            ):
                mesh = make_orientation_mesh(
                    np.float64,
                    args.size,
                    width,
                    fixed,
                    "disconnected" if disconnected else topology,
                    scrambled,
                )
                mesh.BuildCells()
                mesh.BuildLinks()
                row = dict(
                    width=width,
                    fixed=fixed,
                    topology=topology,
                    scrambled=scrambled,
                    disconnected=disconnected,
                    cells=mesh.GetNumberOfCells(),
                )
                for name, cls in (
                    ("orient", vtkOrientPolyData),
                    ("normals", vtkPolyDataNormals),
                ):
                    filter_ = cls()
                    filter_.SetInputData(mesh)
                    row[name + "_ms"], row[name + "_cpu_ms"] = measure(
                        filter_, args.repeats
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
