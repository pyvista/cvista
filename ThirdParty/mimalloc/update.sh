#!/usr/bin/env bash
# fvtk: refresh the vendored mimalloc subtree under ThirdParty/mimalloc/vtkmimalloc.
#
# Unlike the upstream VTK third-party libs (which track Kitware's mirror repos via
# update-common.sh), fvtk vendors mimalloc directly from the upstream GitHub
# release tag below as a plain subtree: src/, include/, and LICENSE. The CMake
# wiring (vtkmimalloc/CMakeLists.txt) and the operator new/delete override
# (Common/Core/vtkFVTKAllocator.cxx) are fvtk-owned and are NOT regenerated here.
#
# To bump the pin: edit `tag`, run this script, and re-run the parity gates.

set -e
set -x

readonly name="mimalloc"
readonly tag="v2.3.2"
readonly subtree="ThirdParty/$name/vtkmimalloc"
readonly repo="https://github.com/microsoft/mimalloc.git"

# Paths copied verbatim from the upstream checkout into the subtree.
readonly paths="
include
src
LICENSE
readme.md
"

here="$(cd "${BASH_SOURCE%/*}/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

git clone --depth 1 --branch "$tag" "$repo" "$tmp/$name"

dest="$here/$subtree"
rm -rf "$dest/include" "$dest/src"
mkdir -p "$dest"
for p in $paths; do
  cp -r "$tmp/$name/$p" "$dest/"
done
# Normalize the upstream readme filename to README.md (kept for provenance).
if [ -f "$dest/readme.md" ]; then
  mv -f "$dest/readme.md" "$dest/README.md"
fi

echo "Updated $subtree to mimalloc $tag"
