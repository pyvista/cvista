#!/usr/bin/env python
"""Regression: a threaded SMP backend must not deadlock against a Python observer.

fvtk defaults the vtkSMPTools backend to STDThread, so hundreds of filters run
their ``vtkSMPTools::For`` loops multithreaded by default. Several filters report
progress/abort from *inside* the parallel functor using VTK's idiom
``if (vtkSMPTools::GetSingleThread()) this->UpdateProgress(...)``. Under the
(default) Sequential backend the designated thread IS the GIL-holding Python
caller, so the ProgressEvent observer runs on the main thread -- fine. Under a
threaded backend that designated thread is a *worker*: it invokes the Python
observer (vtkPythonCommand -> vtkPythonScopeGilEnsurer -> PyGILState_Ensure) and
blocks on the GIL, which the main thread holds while parked on the For join
barrier -> permanent deadlock.

The fix: vtkSMPTools::For drops the GIL on the calling thread (PyEval_SaveThread,
registered via vtkSMPTools::SetGilCallbacks from vtkPythonUtil) for the duration
of a threaded dispatch, so the worker can acquire it to run the callback, then
re-acquires it. Releasing the GIL does not change the computed result.

This reproduced on vtkThreshold and the vtkThreadedImageAlgorithm path. Because
a deadlocked main thread holds the GIL, an in-process Python watchdog thread
cannot fire (it also needs the GIL) -- so the probe runs in a *subprocess* and we
assert it finishes within a generous timeout. Without the fix the subprocess
hangs and TimeoutExpired fails the test.
"""

import subprocess
import sys
import sysconfig
import textwrap

import pytest

# Worst case is a fast machine running this serially; 60s is far above the
# sub-second real runtime but well under any CI step budget, so a hang is
# unambiguous.
TIMEOUT_S = 60

# The child: force STDThread, attach a Python ProgressEvent observer that we can
# confirm actually fired (proving a worker thread really did call back into
# Python), and run a filter that reports progress from inside its SMP functor.
_CHILD = textwrap.dedent(
    """
    import sys

    from fvtk.vtkCommonCore import vtkSMPTools, vtkCommand
    from fvtk.vtkImagingCore import vtkRTAnalyticSource
    from fvtk.vtkFiltersCore import vtkThreshold

    smp = vtkSMPTools()
    smp.SetBackend("STDThread")
    smp.Initialize(4)
    if smp.GetBackend() != "STDThread":
        print("backend not STDThread:", smp.GetBackend())
        sys.exit(3)

    # A non-trivial number of cells so For() actually dispatches to worker
    # threads (grain < n) instead of running inline on the caller.
    src = vtkRTAnalyticSource()
    src.SetWholeExtent(-40, 40, -40, 40, -40, 40)  # 81^3 points
    src.Update()
    image = src.GetOutput()

    progress_calls = {"n": 0}

    thr = vtkThreshold()
    thr.SetInputData(image)
    thr.SetInputArrayToProcess(0, 0, 0, 0, "RTData")
    # Wide band keeps (almost) all cells, so output is guaranteed regardless of
    # the default threshold function -- this test cares about threading+progress,
    # not selectivity.
    if hasattr(thr, "SetThresholdFunction") and hasattr(thr, "THRESHOLD_BETWEEN"):
        thr.SetThresholdFunction(thr.THRESHOLD_BETWEEN)
    thr.SetLowerThreshold(-1.0e9)
    thr.SetUpperThreshold(1.0e9)
    # Observer is a Python callable; under a threaded backend it is invoked from a
    # worker thread, which is exactly the deadlock trigger.
    thr.AddObserver(vtkCommand.ProgressEvent, lambda obj, evt: progress_calls.__setitem__("n", progress_calls["n"] + 1))
    thr.Update()

    out = thr.GetOutput()
    # Sanity: the filter produced output and the Python observer really ran.
    if out.GetNumberOfCells() <= 0:
        print("no output cells")
        sys.exit(4)
    if progress_calls["n"] == 0:
        print("progress observer never fired")
        sys.exit(5)
    print("OK cells=%d progress=%d" % (out.GetNumberOfCells(), progress_calls["n"]))
    sys.exit(0)
    """
)


def test_threaded_smp_does_not_deadlock_on_python_observer():
    if sysconfig.get_config_var("Py_GIL_DISABLED"):
        pytest.skip("free-threaded build: no GIL to contend, deadlock cannot occur")

    try:
        proc = subprocess.run(
            [sys.executable, "-c", _CHILD],
            capture_output=True,
            text=True,
            timeout=TIMEOUT_S,
        )
    except subprocess.TimeoutExpired:
        pytest.fail(
            f"threaded SMP + Python ProgressEvent observer deadlocked "
            f"(no completion within {TIMEOUT_S}s) -- the GIL-release hook is missing or broken"
        )

    assert proc.returncode == 0, (
        f"child failed rc={proc.returncode}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
    )
    assert "OK" in proc.stdout, proc.stdout


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__, "-v"]))
