# Preserve the compiler cache in Linux wheel jobs

The Linux wheel jobs restored and mounted a compiler-cache directory but then
set `CIBW_ENVIRONMENT` to a few build-profile flags. That option replaces the
configured environment table, so it removed `CCACHE_DIR`, the C/C++ compiler
launchers, and the other cache settings from `pyproject.toml`. The builds ran
without the intended cache, saving artifacts of only about 221 bytes.

The normal, LTO, ARM, and two abi3 tiered Linux jobs now set their profile flags
as step environment variables and pass them into the container through
`CIBW_ENVIRONMENT_PASS_LINUX`. The common environment table remains the single
source of cache settings. The tiered jobs still pass their optional version
override. Optimization flags, init-cache selection, architectures, cache
namespaces, smoke tests, and parity gates are unchanged.

This follows cibuildwheel's documented [environment precedence and
pass-through behavior](https://cibuildwheel.pypa.io/en/v3.1.4/options/#environment-pass).
macOS/Windows and the legacy jobs already explicitly preserve their cache or
platform settings and are unaffected.

The pre-build statistics reset also clears `CCACHE_BASEDIR` for that command
alone: its literal `{project}` value is expanded later by the build backend and
is otherwise invalid to ccache. Compilation retains the configured base
directory; this only makes the statistics reset work.

## Validation

Using cibuildwheel 3.1.4's actual options parser, all five affected job profiles
retained `/ccache`, both `ccache` launchers, their original optimization/init-cache
flags, and the tiered version override.

A fresh manylinux build populated the cache. A subsequent clean-build-tree run
of the same source with the persisted cache, using cibuildwheel 3.1.4, passed the
wheel smoke tests and reported 5,309 direct hits, 3 preprocessed hits, and 3
misses: **99.94% hits**. The cache held about 671 MB, and that local wheel build
completed in about two minutes on an i9-14900KF with 24 build jobs. These are
local measurements, not a promise of identical CI timings. The first corrected
CI run must populate its cache before later compatible runs can reuse it.

The cached wheel used the regular PR profile:
`CVISTA_LTO=0 CVISTA_GATE_O2=1 CVISTA_SOURCE_UNITY=0`. Its SHA-256 was
`23e54f3397c95161008a43c22276162e45d5c3854018e047884dacc35e4ddf71`.

That wheel passed all **885 bit-exact tests**, including the 1/4/8-thread
determinism checks, and all **13 RGBA/depth render comparisons** against stock
VTK 9.7.0. Full PyVista validation at pinned commit
`9e85d7045eb7455ac39437cdfea2ef08893b85b8` passed 10,025 core tests and 2,656
plotting tests. Its CLI color-formatting and offscreen-platform-probe failures
were identical to stock's: JUnit failure-node comparison found **zero new
failures**. The cache change does not modify runtime sources.
