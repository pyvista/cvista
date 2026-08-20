#!/usr/bin/env bash
#
# High-repeat SMP parity STRESS harness (dispatch-only diagnostic; NOT a gate).
#
# A real threading divergence (vtkVisualStatistics: a parallel histogram bin
# miscount, serial=1 vs parallel=2) once PASSED a single low-repeat parity gate
# run and only surfaced on a later run -- i.e. intermittent, scheduling-dependent
# VALUE/COUNT divergences slip through one low-REPEATS pass. This harness cranks
# REPEATS *and* loops the whole ~90-filter suite many times to shake such
# intermittent divergences out, so they can be caught (and fixed) rather than
# shipped.
#
# It builds the SAME validator as ci/run-smp-parity.sh (NATIVE release, no
# sanitizers -- value divergences reproduce best at native speed + high repeats)
# and then runs it ITERATIONS times, each with a high REPEATS. The validator
# itself already exercises thread counts {2,4,8,hardware_concurrency*2}
# (oversubscribed) with REPEATS runs per (case,threadcount); looping it re-seeds
# the whole schedule ITERATIONS more times.
#
# Kept a SEPARATE script from run-smp-parity.sh on purpose: the gate script is
# being edited concurrently on a heap-hunt branch, and this stays off its path.
#
# Two failure modes are distinguished:
#   * A gated VALUE/COUNT divergence the validator prints
#     ("FAIL byte-exact vs serial", "FAIL geometry ...", cell/point count deltas,
#      "nondeterministic ...") -> a real intermittent bug to CATCH. Recorded in
#      the catalog; makes this run exit nonzero.
#   * A SEGFAULT (exit 139, "core dumped") -> the SEPARATE, pre-existing
#     heap-corruption crash already under investigation on branch diag/smp-asan.
#     NOT chased here: the loop is resilient to it (each iteration runs with
#     `|| true`) so a crash in one iteration does not hide value-divergences in
#     later iterations. Segfaults are only noted, never counted as divergences.
#
#   Usage: INSTALL_PREFIX=./sdk-install \
#          SMP_STRESS_REPEATS=200 SMP_STRESS_ITERATIONS=8 ci/run-smp-parity-stress.sh
set -uxo pipefail   # NB: no -e; iterations are individually tolerated (segfaults)

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${SRC}/sdk-install}"
BUILD_DIR="${SMP_BUILD_DIR:-${SRC}/build-smp-parity}"
REPEATS="${SMP_STRESS_REPEATS:-200}"
ITERATIONS="${SMP_STRESS_ITERATIONS:-8}"

case "$(uname -s)" in
  Linux*)               CVISTA_OS=linux;   PYBIN="${PYBIN:-/opt/python/cp312-cp312/bin}" ;;
  Darwin*)              CVISTA_OS=macos;   PYBIN="${PYBIN:-$(dirname "$(command -v python3)")}" ;;
  MINGW*|MSYS*|CYGWIN*) CVISTA_OS=windows; PYBIN="${PYBIN:-$(dirname "$(command -v python)")}" ;;
  *) echo "run-smp-parity-stress: unsupported OS $(uname -s)" >&2; exit 1 ;;
esac

cmpath() { if [ "$CVISTA_OS" = windows ]; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

export PATH="$PYBIN:$PATH"

# Core dumps help post-mortem the (separate) segfault if it occurs; harmless otherwise.
ulimit -c unlimited 2>/dev/null || true

VTK_DIR="${VTK_DIR:-$(dirname "$(find "$INSTALL_PREFIX" -name 'vtk-config.cmake' | head -1)")}"
if [ -z "$VTK_DIR" ] || [ ! -f "$VTK_DIR/vtk-config.cmake" ]; then
  echo "::error::run-smp-parity-stress: no vtk-config.cmake under $INSTALL_PREFIX" >&2
  exit 1
fi
echo "using VTK_DIR=$VTK_DIR"

cmake -S "$(cmpath "$SRC/cvista-validate")" -B "$(cmpath "$BUILD_DIR")" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$(cmpath "$(command -v ninja)")" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVTK_DIR="$(cmpath "$VTK_DIR")"
cmake --build "$(cmpath "$BUILD_DIR")" --parallel

EXE="$BUILD_DIR/cvista_smp_parity"
[ "$CVISTA_OS" = windows ] && EXE="$BUILD_DIR/cvista_smp_parity.exe"

case "$CVISTA_OS" in
  linux)   export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${LD_LIBRARY_PATH:-}" ;;
  macos)   export DYLD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${DYLD_LIBRARY_PATH:-}" ;;
  windows) export PATH="$(cygpath -u "$INSTALL_PREFIX/bin"):$PATH" ;;
esac

LOGDIR="$BUILD_DIR/stress-logs"
mkdir -p "$LOGDIR"

echo "=============================================================================="
echo " SMP parity STRESS: $ITERATIONS iterations x REPEATS=$REPEATS per (case,threadcount)"
echo " validator: $EXE"
echo "=============================================================================="

value_divergence_iters=0   # iterations with a gated VALUE/COUNT divergence
segfault_iters=0           # iterations that crashed (separate pre-existing bug)
clean_iters=0

for i in $(seq 1 "$ITERATIONS"); do
  LOG="$LOGDIR/iter-$i.log"
  echo ""
  echo "------------------------------ iteration $i / $ITERATIONS ------------------------------"
  # Resilient: never let one iteration (incl. a segfault) abort the sweep.
  "$EXE" "$REPEATS" >"$LOG" 2>&1
  rc=$?
  tail -n 4 "$LOG" || true
  echo "iteration $i exit code: $rc"

  if [ "$rc" -eq 0 ]; then
    clean_iters=$((clean_iters + 1))
  elif [ "$rc" -ge 128 ]; then
    # 128+N == killed by signal N (139 = SIGSEGV). Pre-existing heap crash; note, do not chase.
    segfault_iters=$((segfault_iters + 1))
    echo "::warning::iteration $i terminated by signal $((rc - 128)) (likely the separate pre-existing SMP heap crash; not a value divergence)"
  else
    # Nonzero, non-signal: the validator itself reported a gated divergence.
    value_divergence_iters=$((value_divergence_iters + 1))
    echo "::warning::iteration $i reported a gated parity divergence (rc=$rc); see catalog"
  fi
done

echo ""
echo "=============================================================================="
echo " STRESS CATALOG"
echo "=============================================================================="
echo "iterations           : $ITERATIONS (repeats/iter: $REPEATS)"
echo "clean iterations     : $clean_iters"
echo "value-divergence itrs: $value_divergence_iters"
echo "segfault iterations  : $segfault_iters (separate pre-existing crash; not chased here)"
echo ""

# Aggregate every distinct gated-divergence row printed across all iterations.
# These are the intermittent bugs to catch (filter + serial-vs-parallel detail).
echo "--- distinct gated FAIL rows (VALUE/COUNT/order divergences) across all iterations ---"
if grep -h -E "FAIL (byte-exact|geometry|geometry unstable|nondeterministic|: no output)" "$LOGDIR"/iter-*.log 2>/dev/null \
     | sort -u | grep . ; then
  :
else
  echo "(none)"
fi
echo ""
echo "--- distinct SERIAL-UNSTABLE rows (single-threaded nondeterminism; separate class) ---"
if grep -h "SERIAL-UNSTABLE" "$LOGDIR"/iter-*.log 2>/dev/null | sort -u | grep . ; then
  :
else
  echo "(none)"
fi
echo ""
echo "--- distinct KNOWN-ISSUE observed rows (documented, ungated) ---"
if grep -h "KNOWN-ISSUE" "$LOGDIR"/iter-*.log 2>/dev/null | sort -u | grep . ; then
  :
else
  echo "(none)"
fi
echo ""

echo "=============================================================================="
if [ "$value_divergence_iters" -gt 0 ]; then
  echo "STRESS RESULT: intermittent gated divergence(s) caught in $value_divergence_iters/$ITERATIONS iterations (see catalog above)"
  exit 1
fi
echo "STRESS RESULT: no gated value/count divergence across $ITERATIONS iterations x $REPEATS repeats"
echo "  (segfault iterations, if any, are the separate pre-existing heap crash tracked on diag/smp-asan)"
exit 0
