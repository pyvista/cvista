#!/usr/bin/env bash
# 3-phase PGO release build of cvista: instrument (CVISTA_PGO=gen), train with
# tools/pgo-train.py, rebuild with the profile (CVISTA_PGO=use + LTO + ICF).
# gen and use share $BUILD, where the .gcda files sit next to the objects.
# Env: REPO (default cwd), BUILD, PYVISTA_DIR (training source; shallow-cloned if
# absent), TRAIN_VENV, TRAIN_PYTHON (default python3).
set -euo pipefail
REPO="${REPO:-$PWD}"
BUILD="${BUILD:-$REPO/build-cvista-pgo}"
PYVISTA_DIR="${PYVISTA_DIR:-$REPO/_pyvista_train}"
TRAIN_VENV="${TRAIN_VENV:-/tmp/cvista-pgo-train-venv}"
TRAIN_PYTHON="${TRAIN_PYTHON:-python3}"

echo "::group::PGO phase 1/3 — instrument build"
CVISTA_PGO=gen CVISTA_ICF=0 pyvista-build cvista --yes --source "$REPO" --build-dir "$BUILD" --opt lto=0 --no-repair
WHEEL_GEN="$(ls "$BUILD"/build/dist/*.whl)"
echo "instrumented wheel: $WHEEL_GEN"
echo "::endgroup::"

[ -d "$PYVISTA_DIR" ] || git clone --depth 1 https://github.com/pyvista/pyvista "$PYVISTA_DIR"
find "$BUILD" -name '*.gcda' -delete 2>/dev/null || true

echo "::group::PGO phase 2/3 — train"
export PYVISTA_OFF_SCREEN=true
"$TRAIN_PYTHON" -m venv "$TRAIN_VENV"
"$TRAIN_VENV/bin/python" -m pip -q install --upgrade pip numpy
"$TRAIN_VENV/bin/python" -m pip -q install "$PYVISTA_DIR"
# Drop the stock `vtk` pyvista pulls in, so training cannot silently use it.
"$TRAIN_VENV/bin/python" -m pip -q uninstall -y vtk 2>/dev/null || true
"$TRAIN_VENV/bin/python" -m pip -q install "$WHEEL_GEN"
SP="$("$TRAIN_VENV/bin/python" -c "import site;print(site.getsitepackages()[0])")"
cp "$REPO/tools/cvista_shim.py" "$SP/_cvista_shim.py"
echo "import _cvista_shim" > "$SP/_cvista_shim.pth"
"$TRAIN_VENV/bin/python" "$REPO/tools/pgo-train.py"
echo "::endgroup::"

N=$(find "$BUILD" -name '*.gcda' | wc -l)
echo "profile data: $N .gcda files"
[ "$N" -ge 100 ] || { echo "ERROR: profile too sparse ($N) — training did not exercise the build"; exit 1; }

echo "::group::PGO phase 3/3 — rebuild with profile (LTO+ICF)"
rm -f "$BUILD/build/CMakeCache.txt"
CVISTA_PGO=use CVISTA_ICF=1 pyvista-build cvista --yes --source "$REPO" --build-dir "$BUILD" --opt lto=1
echo "::endgroup::"

echo "PGO release wheel: $(ls "$BUILD"/wheelhouse/*.whl)"
