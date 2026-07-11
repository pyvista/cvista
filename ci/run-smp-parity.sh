#!/usr/bin/env bash
#
# Build the standalone cvista_smp_parity validator (cvista-validate/) against an
# already-installed cvista via find_package(VTK), then run it. The validator
# exits nonzero if any algorithm's STDThread (parallel) output is not byte-exact
# with its Sequential (serial) output -- i.e. this is the CI gate on the
# "threaded == serial" claim in cvista-config/minimal.cmake.
#
# Expects ci/build-sdk.sh to have populated the install prefix first (default
# ./sdk-install). Mirrors build-sdk.sh's per-OS path/interpreter handling so it
# runs identically on Linux, macOS, and Windows.
#
#   Usage: INSTALL_PREFIX=./sdk-install ci/run-smp-parity.sh
set -euxo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${SRC}/sdk-install}"
BUILD_DIR="${SMP_BUILD_DIR:-${SRC}/build-smp-parity}"
REPEATS="${SMP_PARITY_REPEATS:-4}"

case "$(uname -s)" in
  Linux*)               CVISTA_OS=linux;   PYBIN="${PYBIN:-/opt/python/cp312-cp312/bin}" ;;
  Darwin*)              CVISTA_OS=macos;   PYBIN="${PYBIN:-$(dirname "$(command -v python3)")}" ;;
  MINGW*|MSYS*|CYGWIN*) CVISTA_OS=windows; PYBIN="${PYBIN:-$(dirname "$(command -v python)")}" ;;
  *) echo "run-smp-parity: unsupported OS $(uname -s)" >&2; exit 1 ;;
esac

# CMake/native tools want drive-letter paths on Windows; cygpath -m emits them.
cmpath() { if [ "$CVISTA_OS" = windows ]; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

export PATH="$PYBIN:$PATH" # pip-installed cmake/ninja from the build-sdk step

# Locate the installed VTK CMake package (…/lib/cmake/vtk-9.6/vtk-config.cmake).
VTK_DIR="${VTK_DIR:-$(dirname "$(find "$INSTALL_PREFIX" -name 'vtk-config.cmake' | head -1)")}"
if [ -z "$VTK_DIR" ] || [ ! -f "$VTK_DIR/vtk-config.cmake" ]; then
  echo "::error::run-smp-parity: no vtk-config.cmake under $INSTALL_PREFIX" >&2
  exit 1
fi
echo "using VTK_DIR=$VTK_DIR"

# DIAGNOSTIC: when a CVISTA_{ASAN,TSAN,DEBUGINFO} knob is set the installed VTK tree
# was built with the matching flags (see cvista-config/minimal.cmake). The
# out-of-tree validator is a SEPARATE CMake project, so instrument it to match --
# building the exe with the sanitizer makes libasan/libtsan its DT_NEEDED[0] so the
# runtime initialises first and its own frames symbolise; -g gives the validator
# real line numbers under gdb. Off the normal gate; only the workflow_dispatch
# diagnostic jobs set these knobs.
DIAG_CXX=""
DIAG_LINK=""
if [ "${CVISTA_ASAN:-0}" = "1" ]; then
  DIAG_CXX="-fsanitize=address -fno-omit-frame-pointer -g"; DIAG_LINK="-fsanitize=address -rdynamic"
elif [ "${CVISTA_TSAN:-0}" = "1" ]; then
  DIAG_CXX="-fsanitize=thread -fno-omit-frame-pointer -g";  DIAG_LINK="-fsanitize=thread -rdynamic"
elif [ "${CVISTA_DEBUGINFO:-0}" = "1" ] || [ "${CVISTA_NATIVE_REPRO:-0}" = "1" ]; then
  # -g + -rdynamic so the in-process SIGSEGV handler's backtrace_symbols() and gdb
  # name the validator's own frames (processCase/runOnce/main). In NATIVE-REPRO mode
  # the VTK SDK itself is built WITHOUT -g (plain crashing gate layout), so its frames
  # symbolise at function level via the unstripped .so dynsym (file:line via a -g copy
  # + addr2line offline if needed); adding -g only to the small validator does not
  # shift the VTK .so heap layout, so the crash is preserved.
  DIAG_CXX="-g"; DIAG_LINK="-rdynamic"
fi
DIAG_CMAKE_ARGS=()
[ -n "$DIAG_CXX" ]  && DIAG_CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="$DIAG_CXX")
[ -n "$DIAG_LINK" ] && DIAG_CMAKE_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS="$DIAG_LINK")

cmake -S "$(cmpath "$SRC/cvista-validate")" -B "$(cmpath "$BUILD_DIR")" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$(cmpath "$(command -v ninja)")" \
  -DCMAKE_BUILD_TYPE=Release \
  ${DIAG_CMAKE_ARGS[@]+"${DIAG_CMAKE_ARGS[@]}"} \
  -DVTK_DIR="$(cmpath "$VTK_DIR")"
cmake --build "$(cmpath "$BUILD_DIR")" --parallel

EXE="$BUILD_DIR/cvista_smp_parity"
[ "$CVISTA_OS" = windows ] && EXE="$BUILD_DIR/cvista_smp_parity.exe"

# The out-of-tree exe links the installed VTK shared libs; put them on the
# loader path (bin/ on Windows for the DLLs, lib/ elsewhere for the .so/.dylib).
case "$CVISTA_OS" in
  linux)   export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${LD_LIBRARY_PATH:-}" ;;
  macos)   export DYLD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${DYLD_LIBRARY_PATH:-}" ;;
  windows) export PATH="$(cygpath -u "$INSTALL_PREFIX/bin"):$PATH" ;;
esac

DIAG_LOOPS="${SMP_PARITY_DIAG_LOOPS:-${SMP_PARITY_ASAN_LOOPS:-8}}"

# ---- shared crash-capture plumbing (cores + libSegFault + post-mortem) -------
# The heap OVERFLOW reproduces reliably in exactly two configs: TSan-instrumented,
# and the plain stripped GATE layout. Any -g / ASAN / live-gdb build shifts the heap
# and MASKS it. So we run a KNOWN-CRASHING config natively and capture post-mortem,
# and the validator's compiled-in per-case fork isolation + SIGSEGV handler name the
# offending case+phase deterministically (see cvista-validate/main.cxx).
ulimit -c unlimited 2>/dev/null || true
COREDIR="$BUILD_DIR/cores"; mkdir -p "$COREDIR"
{ echo "$COREDIR/core.%e.%p" > /proc/sys/kernel/core_pattern; } 2>/dev/null \
  || sysctl -w "kernel.core_pattern=$COREDIR/core.%e.%p" 2>/dev/null || true
echo "core_pattern=$(cat /proc/sys/kernel/core_pattern 2>/dev/null || echo '?')"
A2L="$(command -v addr2line || true)"

postMortem() { # $1=logfile -- dump any core's all-thread bt + addr2line libSegFault frames
  local log="$1" core
  core="$(ls -t "$COREDIR"/core* /tmp/core* 2>/dev/null | head -1 || true)"
  if [ -n "$core" ] && [ -e "$core" ]; then
    echo "------------------ POST-MORTEM gdb ($core) ------------------"
    gdb -q -batch "$EXE" "$core" -ex 'set pagination off' \
      -ex 'thread apply all bt full' -ex 'info registers' -ex 'quit' 2>&1 || true
  else
    echo "(no core file produced -- container core_pattern likely host-piped)"
  fi
  if [ -n "$A2L" ]; then
    echo "------------------ addr2line symbolication ------------------"
    grep -aoE "[^ (]+\(\+0x[0-9a-fA-F]+\)" "$log" 2>/dev/null | while IFS='(' read -r mod off; do
      off="${off#+}"; off="${off%)}"
      if [ -e "$mod" ]; then
        printf '%s %s -> ' "$mod" "$off"; "$A2L" -f -C -i -e "$mod" "$off" | tr '\n' ' '; echo
      fi
    done || true
  fi
}
CRASH_MARKERS="CVISTA-PARITY CRASH|CHILD CRASHED|CRASHED CASES|ThreadSanitizer: SEGV|Segmentation fault|Backtrace:|\*\*\* .*signal|Aborted|corrupted|malloc\(\)|free\(\)"

if [ "${CVISTA_ASAN:-0}" = "1" ]; then
  # ASAN: abort at the FIRST memory error (naming the corrupting access). NOTE: ASAN's
  # slowdown perturbs the STDThread schedule and Heisenbug-masks this timing-dependent
  # overflow (an earlier ASAN run was clean); kept for completeness.
  export ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:detect_leaks=0:detect_odr_violation=0:print_stacktrace=1:symbolize=1:handle_segv=1"
  command -v addr2line >/dev/null 2>&1 && export ASAN_SYMBOLIZER_PATH="$(command -v addr2line)"
  echo "== ASAN diagnostic run: ASAN_OPTIONS=$ASAN_OPTIONS =="
  for i in $(seq 1 "$DIAG_LOOPS"); do
    echo "===== ASAN sweep $i/$DIAG_LOOPS (repeats=$REPEATS) ====="
    "$EXE" "$REPEATS"
  done
  echo "== ASAN diagnostic: completed $DIAG_LOOPS clean sweeps with no memory error =="

elif [ "${CVISTA_TSAN:-0}" = "1" ]; then
  # TSan is one of the two configs that CRASH RELIABLY. Run IN-PROCESS (TSan + fork()
  # is unsupported): when the process SIGSEGVs, the validator's compiled-in handler
  # prints the case+phase+backtrace, and the -g TSan build + a core gives file:line.
  # (TSan may also emit its own SEGV/race report.) handle_segv=0 so OUR handler runs.
  export TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1:history_size=7:handle_segv=0:abort_on_error=0"
  echo "== TSan diagnostic (in-process; handler attribution): up to $DIAG_LOOPS sweeps =="
  crashed=0
  for i in $(seq 1 "$DIAG_LOOPS"); do
    echo "===== TSan sweep $i/$DIAG_LOOPS (repeats=$REPEATS) ====="
    log="$COREDIR/tsan_$i.log"; rc=0
    ( cd "$COREDIR" && "$EXE" "$REPEATS" ) > "$log" 2>&1 || rc=$?
    tail -4 "$log" 2>/dev/null || true
    if [ "$rc" -ge 128 ] || grep -qE "$CRASH_MARKERS|WARNING: ThreadSanitizer" "$log"; then
      echo "!!! TSan-config CRASH/REPORT on sweep $i (rc=$rc, signal $((rc>=128?rc-128:0))) !!!"
      cat "$log"; postMortem "$log"; crashed=1; break
    fi
  done
  if [ "$crashed" = 0 ]; then echo "== TSan diagnostic: $DIAG_LOOPS sweeps, no crash/report =="; else exit 1; fi

elif [ "${CVISTA_DEBUGINFO:-0}" = "1" ] || [ "${CVISTA_NATIVE_REPRO:-0}" = "1" ]; then
  # NATIVE full-speed run of a KNOWN-CRASHING layout (NATIVE-REPRO = the plain GATE
  # build, no -g on VTK). LD_PRELOAD libSegFault for a symbol-level backtrace with zero
  # timing perturbation; per-case fork isolation (CVISTA_FORK_ISOLATE=1) attributes the
  # crash to one case+phase; cores give a post-mortem all-thread bt.
  SEGF=""
  for p in /lib64/libSegFault.so /usr/lib64/libSegFault.so \
           /lib/x86_64-linux-gnu/libSegFault.so /usr/lib/libSegFault.so; do
    [ -e "$p" ] && SEGF="$p" && break
  done
  [ -n "$SEGF" ] && echo "libSegFault: $SEGF" || echo "libSegFault.so not found (relying on cores)"
  export SEGFAULT_SIGNALS=all
  echo "== native full-speed diagnostic (fork=${CVISTA_FORK_ISOLATE:-0}): up to $DIAG_LOOPS sweeps =="
  crashed=0
  for i in $(seq 1 "$DIAG_LOOPS"); do
    echo "===== native sweep $i/$DIAG_LOOPS ====="
    log="$COREDIR/native_$i.log"; rc=0
    if [ -n "$SEGF" ]; then
      ( cd "$COREDIR" && LD_PRELOAD="$SEGF" "$EXE" "$REPEATS" ) > "$log" 2>&1 || rc=$?
    else
      ( cd "$COREDIR" && "$EXE" "$REPEATS" ) > "$log" 2>&1 || rc=$?
    fi
    tail -3 "$log" 2>/dev/null || true
    if [ "$rc" -ge 128 ] || grep -qE "$CRASH_MARKERS" "$log"; then
      echo "!!! NATIVE CRASH/ATTRIBUTION on sweep $i (rc=$rc) !!!"
      cat "$log"; postMortem "$log"; crashed=1; break
    fi
  done
  if [ "$crashed" = 0 ]; then echo "== native diagnostic: $DIAG_LOOPS sweeps, NO crash reproduced =="; else exit 1; fi

else
  "$EXE" "$REPEATS"
fi
