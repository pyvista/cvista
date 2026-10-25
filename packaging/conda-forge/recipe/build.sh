#!/usr/bin/env bash
# Conda-forge Unix (Linux & macOS) build script for cvista 9.7.0.x.
#
# Drives CMake + Ninja against cvista-config/minimal.cmake with unvendored
# conda-forge ThirdParty libraries (VTK_USE_EXTERNAL=ON), installs the
# top-level `cvista` Python package into ${SP_DIR}/cvista and `libvtk*-cvista`
# shared libraries into ${PREFIX}/lib, generates `cvista/_class_index.py` via
# `cvista/generate_index.py`, and writes `.dist-info` / `.egg-info` metadata.
set -euxo pipefail

SRC_DIR="${SRC_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
BUILD_DIR="${BUILD_DIR:-${SRC_DIR}/build-conda}"
PREFIX="${PREFIX:?PREFIX must be set by conda-build / rattler-build}"
PYTHON="${PYTHON:-$(command -v python3 || command -v python)}"

PY_VER="${PY_VER:-$("${PYTHON}" -c 'import sys; print(f"{sys.version_info[0]}.{sys.version_info[1]}")')}"
PYTHON_MAJOR_VERSION="${PY_VER%%.*}"
SP_DIR="${SP_DIR:-$("${PYTHON}" -c 'import sysconfig; print(sysconfig.get_path("platlib"))')}"
SITE_PACKAGES_REL="${SP_DIR#"${PREFIX}/"}"
if [[ "${SITE_PACKAGES_REL}" == "${SP_DIR}" ]]; then
  SITE_PACKAGES_REL="lib/python${PY_VER}/site-packages"
fi

PKG_VERSION="${PKG_VERSION:-9.7.0.0}"
VTK_BASE_VERSION="9.7.0"
if [[ "${PKG_VERSION}" == "${VTK_BASE_VERSION}" ]]; then
  VTK_VERSION_SUFFIX=""
elif [[ "${PKG_VERSION}" == "${VTK_BASE_VERSION}."* ]]; then
  VTK_VERSION_SUFFIX="${PKG_VERSION#"${VTK_BASE_VERSION}."}"
else
  VTK_VERSION_SUFFIX="0"
fi

export SETUPTOOLS_SCM_PRETEND_VERSION="${PKG_VERSION}"
export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_CVISTA="${PKG_VERSION}"

# Disable wheel-specific LTO / mold / ICF levers by default in conda builds so
# conda-forge's native toolchain and linker wrappers behave predictably across
# platforms and fit within CI memory/time budgets.
export CVISTA_LTO="${CVISTA_LTO:-0}"
export CVISTA_FAST_LINKER="${CVISTA_FAST_LINKER:-off}"
export CVISTA_ICF="${CVISTA_ICF:-0}"
export CVISTA_SOURCE_UNITY="${CVISTA_SOURCE_UNITY:-0}"

TARGET_PLATFORM="${target_platform:-}"
if [[ -z "${TARGET_PLATFORM}" ]]; then
  case "$(uname -s)" in
    Linux*)  TARGET_PLATFORM="linux-64" ;;
    Darwin*) TARGET_PLATFORM="osx-64" ;;
    *)       echo "Unsupported Unix host: $(uname -s)" >&2; exit 1 ;;
  esac
fi

PLATFORM_ARGS=()
if [[ "${TARGET_PLATFORM}" == linux-* ]]; then
  PLATFORM_ARGS+=(
    "-DVTK_USE_X:BOOL=ON"
    "-DVTK_OPENGL_HAS_EGL:BOOL=ON"
    "-DVTK_OPENGL_HAS_OSMESA:BOOL=${CVISTA_ENABLE_OSMESA:-ON}"
    "-DVTK_USE_COCOA:BOOL=OFF"
    "-DVTK_DEFAULT_RENDER_WINDOW_OFFSCREEN:BOOL=OFF"
    "-DVTK_DEFAULT_RENDER_WINDOW_HEADLESS:BOOL=OFF"
    "-DVTK_MODULE_USE_EXTERNAL_VTK_gl2ps:BOOL=${CVISTA_EXTERNAL_GL2PS:-ON}"
  )
  if [[ -f "${PREFIX}/lib/libGL.so.1" ]]; then
    PLATFORM_ARGS+=("-DOPENGL_opengl_LIBRARY:FILEPATH=${PREFIX}/lib/libGL.so.1")
  fi
  if [[ -f "${PREFIX}/lib/libEGL.so.1" ]]; then
    PLATFORM_ARGS+=("-DOPENGL_egl_LIBRARY:FILEPATH=${PREFIX}/lib/libEGL.so.1")
  fi
elif [[ "${TARGET_PLATFORM}" == osx-* ]]; then
  export CVISTA_TARGET_OS="macos"
  export CFLAGS="${CFLAGS:-} -Wno-incompatible-pointer-types"
  export CXXFLAGS="${CXXFLAGS:-} -Wno-incompatible-pointer-types"
  PLATFORM_ARGS+=(
    "-DVTK_USE_COCOA:BOOL=ON"
    "-DVTK_USE_X:BOOL=OFF"
    "-DVTK_OPENGL_HAS_EGL:BOOL=OFF"
    "-DVTK_OPENGL_HAS_OSMESA:BOOL=OFF"
    "-DVTK_DEFAULT_RENDER_WINDOW_OFFSCREEN:BOOL=OFF"
    "-DVTK_DEFAULT_RENDER_WINDOW_HEADLESS:BOOL=OFF"
    # Keep gl2ps internal on macOS so it does not pull Mesa X11 GL against Cocoa.
    "-DVTK_MODULE_USE_EXTERNAL_VTK_gl2ps:BOOL=OFF"
  )
  if [[ -n "${CONDA_BUILD_SYSROOT:-}" ]]; then
    PLATFORM_ARGS+=("-DCMAKE_OSX_SYSROOT:PATH=${CONDA_BUILD_SYSROOT}")
  fi
fi

# When cross-compiling without an emulator, skip build-time execution of the
# target-arch vtkpython binary for _class_index.py / .pyi generation.
if [[ "${CONDA_BUILD_CROSS_COMPILATION:-0}" == "1" && -z "${CMAKE_CROSSCOMPILING_EMULATOR:-}" ]]; then
  BUILD_CLASS_INDEX="OFF"
else
  BUILD_CLASS_INDEX="ON"
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Generate a thin CMake initial-cache wrapper that seeds CMAKE_C_FLAGS,
# CMAKE_CXX_FLAGS, and CMAKE_*_LINKER_FLAGS from conda-build's environment
# BEFORE cvista-config/minimal.cmake appends its section-GC / dead-strip flags,
# and keeps the 3 SceneGraph / WebGLExporter leaf classes in the compiled set.
MINIMAL_CMAKE="${SRC_DIR}/cvista-config/minimal.cmake"
cat > "${BUILD_DIR}/conda-init.cmake" <<EOF
set(CMAKE_C_FLAGS "\$ENV{CFLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "\$ENV{CXXFLAGS}" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "\$ENV{LDFLAGS}" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS "\$ENV{LDFLAGS}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS "\$ENV{LDFLAGS}" CACHE STRING "")
include("${MINIMAL_CMAKE}")
list(REMOVE_ITEM CVISTA_NOCOMPILE_CLASSES
  vtkCameraNode
  vtkLightNode
  vtkDiscretizableColorTransferFunction)
set(CVISTA_NOCOMPILE_CLASSES "\${CVISTA_NOCOMPILE_CLASSES}"
    CACHE INTERNAL "cvista: classes dropped from the C++ build (pyvista-unused orphans)" FORCE)
EOF

# Unbundled vs vendored ThirdParty policy (derived from cvista's 133-module closure):
#   External (conda-forge host packages):
#     zlib, libpng (png), libjpeg-turbo (jpeg), libtiff (tiff), freetype, expat,
#     lz4-c (lz4), xz / liblzma-devel (lzma), pugixml, jsoncpp,
#     nlohmann_json (nlohmannjson), fmt, fast_float, eigen, utfcpp (utf8),
#     hdf5, libnetcdf / netcdf4 (netcdf), libxml2, gl2ps (Linux/Win).
#   Vendored in-tree (no compatible external CMake package or cvista-patched):
#     cgns, diy2, exodusII, exprtk, glad, kissfft, loguru, pegtl, scn, token,
#     verdict, xdmf2.
THIRDPARTY_ARGS=(
  "-DVTK_USE_EXTERNAL:BOOL=${CVISTA_USE_EXTERNAL:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_zlib:BOOL=${CVISTA_EXTERNAL_ZLIB:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_png:BOOL=${CVISTA_EXTERNAL_PNG:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_jpeg:BOOL=${CVISTA_EXTERNAL_JPEG:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_tiff:BOOL=${CVISTA_EXTERNAL_TIFF:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_freetype:BOOL=${CVISTA_EXTERNAL_FREETYPE:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_expat:BOOL=${CVISTA_EXTERNAL_EXPAT:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_lz4:BOOL=${CVISTA_EXTERNAL_LZ4:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_lzma:BOOL=${CVISTA_EXTERNAL_LZMA:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_pugixml:BOOL=${CVISTA_EXTERNAL_PUGIXML:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_jsoncpp:BOOL=${CVISTA_EXTERNAL_JSONCPP:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_nlohmannjson:BOOL=${CVISTA_EXTERNAL_NLOHMANNJSON:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_fmt:BOOL=${CVISTA_EXTERNAL_FMT:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_fast_float:BOOL=${CVISTA_EXTERNAL_FAST_FLOAT:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_eigen:BOOL=${CVISTA_EXTERNAL_EIGEN:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_utf8:BOOL=${CVISTA_EXTERNAL_UTF8:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_hdf5:BOOL=${CVISTA_EXTERNAL_HDF5:-ON}"
  "-DHDF5_IS_PARALLEL:BOOL=OFF"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_netcdf:BOOL=${CVISTA_EXTERNAL_NETCDF:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_libxml2:BOOL=${CVISTA_EXTERNAL_LIBXML2:-ON}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_cgns:BOOL=${CVISTA_EXTERNAL_CGNS:-OFF}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_exprtk:BOOL=${CVISTA_EXTERNAL_EXPRTK:-OFF}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_pegtl:BOOL=${CVISTA_EXTERNAL_PEGTL:-OFF}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_scn:BOOL=${CVISTA_EXTERNAL_SCN:-OFF}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_token:BOOL=${CVISTA_EXTERNAL_TOKEN:-OFF}"
  "-DVTK_MODULE_USE_EXTERNAL_VTK_verdict:BOOL=${CVISTA_EXTERNAL_VERDICT:-OFF}"
)

# shellcheck disable=SC2086
cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" -G Ninja \
  ${CMAKE_ARGS:-} \
  -C "${BUILD_DIR}/conda-init.cmake" \
  -Wno-dev \
  -DCMAKE_BUILD_TYPE:STRING=Release \
  -DCMAKE_PREFIX_PATH:PATH="${PREFIX}" \
  -DCMAKE_FIND_FRAMEWORK:STRING=LAST \
  -DCMAKE_INSTALL_PREFIX:PATH="${PREFIX}" \
  -DCMAKE_INSTALL_LIBDIR:STRING=lib \
  -DCMAKE_INSTALL_INCLUDEDIR:STRING=include/cvista \
  -DCMAKE_INSTALL_LICENSEDIR:STRING=share/licenses/cvista \
  -DCMAKE_INSTALL_RPATH:PATH="${PREFIX}/lib" \
  -DBUILD_SHARED_LIBS:BOOL=ON \
  -DVTK_WHEEL_BUILD:BOOL=OFF \
  -DCVISTA_ABI3:BOOL="${CVISTA_ABI3:-OFF}" \
  -DCVISTA_CLASS_INDEX:BOOL="${BUILD_CLASS_INDEX}" \
  -DVTK_WRAP_PYTHON:BOOL=ON \
  -DVTK_PYTHON_VERSION:STRING="${PYTHON_MAJOR_VERSION}" \
  -DPython3_EXECUTABLE:FILEPATH="${PYTHON}" \
  -DPython3_FIND_STRATEGY:STRING=LOCATION \
  -DVTK_PYTHON_OPTIONAL_LINK:BOOL=ON \
  -DVTK_PYTHON_SITE_PACKAGES_SUFFIX:STRING="${SITE_PACKAGES_REL}" \
  -DVTK_CUSTOM_LIBRARY_SUFFIX:STRING=cvista \
  -DVTK_VERSION_SUFFIX:STRING="${VTK_VERSION_SUFFIX}" \
  -DVTK_INSTALL_PYTHON_EXES:BOOL=OFF \
  -DVTK_INSTALL_SDK:BOOL="${CVISTA_INSTALL_SDK:-OFF}" \
  -Dvtk_cmake_destination:STRING=lib/cmake/cvista \
  -Dvtk_hierarchy_destination_args:STRING="HIERARCHY_DESTINATION;lib/cvista/hierarchy/VTK" \
  -DVTK_BUILD_PYI_FILES:BOOL=OFF \
  -DVTK_BUILD_TESTING:BOOL=OFF \
  -DVTK_BUILD_DOCUMENTATION:BOOL=OFF \
  -DVTK_BUILD_EXAMPLES:BOOL=OFF \
  -DVTK_WRAP_SERIALIZATION:BOOL=OFF \
  -DVTK_HAS_FEENABLEEXCEPT:BOOL=OFF \
  "${THIRDPARTY_ARGS[@]}" \
  "${PLATFORM_ARGS[@]}"

JOBS="${CPU_COUNT:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
cmake --install "${BUILD_DIR}"

# Prune dead UI helper subpackages (gtk, tk, wx, test) from the installed
# cvista package, keeping cvista/qt for pyvistaqt compatibility (gh-142).
for subpkg in gtk tk wx test; do
  rm -rf "${SP_DIR}/cvista/${subpkg}"
done

# Ensure cvista/_class_index.py (flat lazy namespace `from cvista import vtkPolyData`)
# is generated and verified in ${SP_DIR}/cvista via cvista/generate_index.py.
if [[ "${BUILD_CLASS_INDEX}" == "ON" ]]; then
  PYTHONPATH="${SP_DIR}:${PYTHONPATH:-}" "${PYTHON}" - <<'PY'
import pathlib
import re
import cvista
import cvista.generate_index

pkg_dir = pathlib.Path(cvista.__file__).resolve().parent
all_py = (pkg_dir / "all.py").read_text(encoding="utf-8")
modules = re.findall(r"^from \.(vtk\w+) import \*", all_py, flags=re.MULTILINE)
if not modules:
    raise RuntimeError(f"No wrapped VTK modules found in {pkg_dir / 'all.py'}")
rc = cvista.generate_index.main(["-p", "cvista", "-o", str(pkg_dir), *modules])
if rc != 0:
    raise SystemExit(rc)
index_file = pkg_dir / "_class_index.py"
assert index_file.is_file(), f"Missing generated {index_file}"
PY
fi

# Write PEP 376 .dist-info and legacy .egg-info metadata so `pip list`,
# `pip check`, and `importlib.metadata.version("cvista")` discover the conda
# package just like a wheel install.
DIST_INFO="${SP_DIR}/cvista-${PKG_VERSION}.dist-info"
rm -rf "${DIST_INFO}"
mkdir -p "${DIST_INFO}"

cat > "${DIST_INFO}/METADATA" <<EOF
Metadata-Version: 2.1
Name: cvista
Version: ${PKG_VERSION}
Summary: Trimmed PyVista-parity fork of VTK 9.7.0 (top-level cvista namespace)
Home-page: https://github.com/pyvista/cvista
License: BSD-3-Clause
Requires-Python: >=3.11
Requires-Dist: matplotlib>=2.0.0
Requires-Dist: numpy>=1.9
EOF

cat > "${DIST_INFO}/INSTALLER" <<EOF
conda
EOF

cat > "${DIST_INFO}/top_level.txt" <<EOF
cvista
EOF

: > "${DIST_INFO}/RECORD"

cat > "${SP_DIR}/cvista-${PKG_VERSION}.egg-info" <<EOF
Metadata-Version: 2.1
Name: cvista
Version: ${PKG_VERSION}
Summary: Trimmed PyVista-parity fork of VTK 9.7.0 (top-level cvista namespace)
Platform: UNKNOWN
EOF
