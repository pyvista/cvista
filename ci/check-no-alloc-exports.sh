#!/usr/bin/env bash
# fvtk: "no host interposition" safety gate.
#
# Asserts that NONE of the fvtk shared objects export an allocator symbol in
# their DYNAMIC symbol table (`nm -D`). fvtk routes its own C++ operator
# new/delete to a vendored static mimalloc (Common/Core/vtkFVTKAllocator.cxx),
# but that override MUST stay fvtk-internal: it is built with hidden visibility
# + -fno-semantic-interposition so it binds within fvtk's own .so files and is
# NEVER placed in .dynsym. If any of malloc/free/calloc/realloc/operator
# new/operator delete ever appears as an EXPORTED (defined, non-UNDEF) dynamic
# symbol, fvtk would interpose the host CPython allocator (and every other
# loaded extension's allocator) -- exactly the LD_PRELOAD-style behavior we
# forbid. This guard fails the build in that case.
#
# Usage:
#   ci/check-no-alloc-exports.sh <wheel-or-dir-or-so> [more ...]
# Accepts a repaired .whl (unzipped + scanned), a directory (scanned for *.so*),
# or individual .so files. Designed to be chained after `auditwheel repair` in
# the cibuildwheel repair-wheel-command, scanning the REPAIRED wheel.

set -euo pipefail

# Exported-symbol patterns that would mean fvtk is interposing the host. We match
# on the C++-mangled operator new/delete (_Znw/_Zna/_Zdl/_Zda) and the C malloc
# family. We only care about DEFINED, EXPORTED dynamic symbols (nm -D shows type
# letters T/W/B/D/R/i etc.; 'U' = undefined import, which is fine and expected --
# fvtk's .so legitimately IMPORTS malloc/operator new from libc/libstdc++).
#
# Mangled global operator new / new[] / delete / delete[] (incl. sized/aligned/
# nothrow variants) all begin with these prefixes:
#   _Znwm _Znwj _Znam _Znaj  (operator new / new[])
#   _ZdlPv _ZdaPv            (operator delete / delete[])
ALLOC_REGEX='^(malloc|free|calloc|realloc|reallocarray|posix_memalign|aligned_alloc|memalign|valloc|pvalloc|_Znw[mj].*|_Zna[mj].*|_ZdlPv.*|_ZdaPv.*)$'

scan_so() {
  local so="$1"
  # nm -D: dynamic symbols. Columns: [addr] type name. Keep only DEFINED exports
  # (type letter is NOT 'U' and NOT 'w'/'v' weak-undef). A defined symbol has an
  # address; undefined ('U') has none. Use --defined-only to be explicit.
  local hits
  hits="$(nm -D --defined-only "$so" 2>/dev/null \
    | awk '{print $NF}' \
    | grep -E "$ALLOC_REGEX" || true)"
  if [[ -n "$hits" ]]; then
    echo "ERROR: $so EXPORTS allocator symbol(s) in .dynsym (host interposition!):" >&2
    echo "$hits" | sed 's/^/    /' >&2
    return 1
  fi
  return 0
}

collect_and_scan_dir() {
  local dir="$1"
  local rc=0
  local found=0
  while IFS= read -r -d '' so; do
    found=1
    scan_so "$so" || rc=1
  done < <(find "$dir" -type f \( -name '*.so' -o -name '*.so.*' \) -print0)
  if [[ "$found" -eq 0 ]]; then
    echo "WARNING: no .so files found under $dir to scan." >&2
  fi
  return "$rc"
}

main() {
  if [[ "$#" -eq 0 ]]; then
    echo "usage: $0 <wheel|dir|so> [...]" >&2
    exit 2
  fi
  local rc=0
  local tmp
  for arg in "$@"; do
    case "$arg" in
      *.whl)
        tmp="$(mktemp -d)"
        unzip -q -o "$arg" -d "$tmp"
        collect_and_scan_dir "$tmp" || rc=1
        rm -rf "$tmp"
        ;;
      *.so|*.so.*)
        scan_so "$arg" || rc=1
        ;;
      *)
        if [[ -d "$arg" ]]; then
          collect_and_scan_dir "$arg" || rc=1
        else
          echo "WARNING: skipping unrecognized argument: $arg" >&2
        fi
        ;;
    esac
  done
  if [[ "$rc" -ne 0 ]]; then
    echo "no-alloc-exports gate FAILED: fvtk must not export allocator symbols." >&2
    exit 1
  fi
  echo "no-alloc-exports gate OK: no exported malloc/free/operator new/delete in fvtk .so's."
}

main "$@"
