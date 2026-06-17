"""Across-thread-count determinism for the fvtk multicore-by-default filters.

fvtk threads a small audited set of bit-exact-safe filters by default (capped at
4 threads); see Common/Core/vtkFVTKSMPDefaults.{h,cxx} and README lever 15. This
test proves the threading is deterministic: it runs the same operations under the
fvtk python at VTK_SMP_MAX_THREADS in {1, 4, 8} and asserts the dumped output is
byte-for-byte identical across all thread counts. Combined with the main
bit-exactness suite (which compares the default 4-thread fvtk against serial stock
VTK 9.6.2), this shows the enabled filters are bit-identical at 1/4/8 threads AND
identical to stock.

Only needs the fvtk python (BITEXACT_FVTK_PY); skips cleanly if unset.
"""
from __future__ import annotations

import os
import subprocess
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
RUN_OPS = os.path.join(HERE, "run_ops.py")
sys.path.insert(0, HERE)
import compare as _compare  # noqa: E402

# Ops whose filters opt into fvtk default-on threading. Exercising any of these
# at >1 thread must produce byte-identical output to the 1-thread run.
THREADED_OPS = ["warp", "warpvector", "normals", "elevation"]

THREAD_COUNTS = [1, 4, 8]


def _env(ldlp, nthreads):
    env = dict(os.environ)
    if ldlp:
        existing = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = ldlp + (":" + existing if existing else "")
    env["VTK_SMP_MAX_THREADS"] = str(nthreads)
    return env


@pytest.fixture(scope="module")
def thread_runs(tmp_path_factory):
    fvtk_py = os.environ.get("BITEXACT_FVTK_PY")
    if not fvtk_py:
        pytest.skip("BITEXACT_FVTK_PY not set; cannot run SMP determinism test.")
    fvtk_ldlp = os.environ.get("BITEXACT_FVTK_LDLP", "")
    base = str(tmp_path_factory.mktemp("smp_determinism"))
    dirs = {}
    for n in THREAD_COUNTS:
        outdir = os.path.join(base, f"t{n}")
        os.makedirs(outdir, exist_ok=True)
        proc = subprocess.run(
            [fvtk_py, RUN_OPS, outdir],
            env=_env(fvtk_ldlp, n),
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"fvtk run at {n} threads failed (rc={proc.returncode}):\n"
                f"STDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
            )
        dirs[n] = outdir
    return dirs


@pytest.mark.parametrize("nthreads", [n for n in THREAD_COUNTS if n != 1])
@pytest.mark.parametrize("op_name", THREADED_OPS)
def test_threaded_filter_is_thread_count_invariant(thread_runs, op_name, nthreads):
    """Output of a default-threaded filter is byte-identical at 1 vs N threads."""
    ref = thread_runs[1]
    other = thread_runs[nthreads]
    res = _compare.compare_all(ref, other)
    bad = [
        k
        for k, v in res["cases"].items()
        if k.startswith(op_name + "__") and not v["ok"]
    ]
    assert not bad, (
        f"{op_name}: output differs between 1 and {nthreads} threads "
        f"(non-deterministic threading): {bad}"
    )
