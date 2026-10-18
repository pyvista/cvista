# Conda-Forge Packaging for `cvista` (`9.7.0.x`)

This directory contains the conda-forge recipe for **`cvista`** (tracking [`pyvista/cvista#140`](https://github.com/pyvista/cvista/issues/140)), ready for local verification with `rattler-build` or `conda-build` and submission to [`conda-forge/staged-recipes`](https://github.com/conda-forge/staged-recipes).

## Contents

| File | Purpose |
|---|---|
| `recipe/recipe.yaml` | v1 `rattler-build` recipe format (used by modern `conda-forge/staged-recipes` and `conda-forge/vtk-feedstock`). |
| `recipe/meta.yaml` | Classic `conda-build` recipe format with identical dependency, platform, and test specifications. |
| `recipe/build.sh` | Unix (`linux-64`, `linux-aarch64`, `osx-64`, `osx-arm64`) CMake + Ninja + flat-index (`cvista.generate_index`) build driver. |
| `recipe/bld.bat` | Windows (`win-64`, MSVC + Win32 WGL) CMake + Ninja + flat-index (`cvista.generate_index`) build driver. |
| `recipe/conda_build_config.yaml` | Platform SDK floor and channel configuration matching `conda-forge/vtk-feedstock`. |

---

## Key Design Decisions

### 1. Native Unvendored Stack (`VTK_USE_EXTERNAL=ON`)

PyPI wheels bundle third-party libraries (`zlib`, `libpng`, `freetype`, `hdf5`, `netcdf`, etc.) via `auditwheel` / `delocate` / `delvewheel`. In a conda environment, loading wheel-bundled native libraries alongside conda-managed libraries causes duplicate symbol resolution and ABI clashes.

The conda-forge build sets `-DVTK_USE_EXTERNAL:BOOL=ON` and links against conda-forge's native packages for every external-capable module in `cvista`'s 133-module minimal closure (`cvista-config/_modules_minimal.cmake`):

| VTK Module | Toggle | Conda-Forge Host Package(s) |
|---|---|---|
| `VTK::zlib` | `-DVTK_MODULE_USE_EXTERNAL_VTK_zlib=ON` | `zlib` |
| `VTK::png` | `-DVTK_MODULE_USE_EXTERNAL_VTK_png=ON` | `libpng` |
| `VTK::jpeg` | `-DVTK_MODULE_USE_EXTERNAL_VTK_jpeg=ON` | `libjpeg-turbo` |
| `VTK::tiff` | `-DVTK_MODULE_USE_EXTERNAL_VTK_tiff=ON` | `libtiff` |
| `VTK::freetype` | `-DVTK_MODULE_USE_EXTERNAL_VTK_freetype=ON` | `freetype` |
| `VTK::expat` | `-DVTK_MODULE_USE_EXTERNAL_VTK_expat=ON` | `expat` |
| `VTK::lz4` | `-DVTK_MODULE_USE_EXTERNAL_VTK_lz4=ON` | `lz4-c` |
| `VTK::lzma` | `-DVTK_MODULE_USE_EXTERNAL_VTK_lzma=ON` | `liblzma-devel`, `xz` |
| `VTK::pugixml` | `-DVTK_MODULE_USE_EXTERNAL_VTK_pugixml=ON` | `pugixml` |
| `VTK::jsoncpp` | `-DVTK_MODULE_USE_EXTERNAL_VTK_jsoncpp=ON` | `jsoncpp` |
| `VTK::nlohmannjson` | `-DVTK_MODULE_USE_EXTERNAL_VTK_nlohmannjson=ON` | `nlohmann_json` |
| `VTK::fmt` | `-DVTK_MODULE_USE_EXTERNAL_VTK_fmt=ON` | `fmt` |
| `VTK::fast_float` | `-DVTK_MODULE_USE_EXTERNAL_VTK_fast_float=ON` | `fast_float >=8.0.0` |
| `VTK::eigen` | `-DVTK_MODULE_USE_EXTERNAL_VTK_eigen=ON` | `eigen`, `eigen-abi-devel` |
| `VTK::utf8` | `-DVTK_MODULE_USE_EXTERNAL_VTK_utf8=ON` | `utfcpp` |
| `VTK::hdf5` | `-DVTK_MODULE_USE_EXTERNAL_VTK_hdf5=ON` (`-DHDF5_IS_PARALLEL=OFF`) | `hdf5` (`nompi_*`) |
| `VTK::netcdf` | `-DVTK_MODULE_USE_EXTERNAL_VTK_netcdf=ON` | `libnetcdf` (`nompi_*`), `netcdf4` |
| `VTK::libxml2` | `-DVTK_MODULE_USE_EXTERNAL_VTK_libxml2=ON` | `libxml2`, `libxml2-devel` |
| `VTK::gl2ps` | `ON` (Linux/Windows), `OFF` (macOS) | `gl2ps` (`[not osx]`) |

The remaining `ThirdParty` modules in `cvista`'s closure either carry `cvista`/VTK-specific patches or have no external CMake package mode in VTK 9.7.0, and are built in-tree with their licenses declared in `about.license_file`:
- Explicitly kept internal via `-DVTK_MODULE_USE_EXTERNAL_VTK_<name>=OFF` (matching `conda-forge/vtk-feedstock` 9.7.0): `cgns`, `exprtk`, `pegtl`, `scn`, `token`, `verdict`.
- Internal-only VTK modules (`vtk_module_third_party_internal`): `diy2`, `exodusII`, `glad`, `kissfft`, `loguru`, `xdmf2`.

### 2. Coexistence with `conda-forge/vtk`

A single conda environment can install both `vtk` and `cvista` without file or symbol collisions:
- **Python package namespace:** `CMakeLists.txt` wraps Python modules into the top-level `cvista` package (`${SP_DIR}/cvista`) rather than `vtkmodules`.
- **Shared library suffix:** `-DVTK_CUSTOM_LIBRARY_SUFFIX:STRING=cvista` names all installed C++ libraries `libvtk*-cvista.so*` / `libvtk*-cvista.dylib` / `vtk*-cvista.dll`, avoiding SONAME and filename collisions with `libvtk*-9.7.so` from `conda-forge/vtk`.
- **No conflicting executables or CMake exports:** `-DVTK_INSTALL_PYTHON_EXES:BOOL=OFF`, `-DVTK_INSTALL_SDK:BOOL=OFF`, and `cvista`-scoped destinations (`lib/cmake/cvista`, `include/cvista`, `share/licenses/cvista`) ensure `bin/vtkpython` and `lib/cmake/vtk-9.7` from `vtk` are untouched.

### 3. Per-Python Builds (`CVISTA_ABI3=OFF`) & `VTK_PYTHON_OPTIONAL_LINK`

- Conda-forge's CI matrix builds per-Python minor variants (`3.11`, `3.12`, `3.13`, `3.14`), so `-DVTK_WHEEL_BUILD:BOOL=OFF` and `-DCVISTA_ABI3:BOOL=OFF` are used to emit standard per-interpreter extension modules with static CPython types.
- On Linux and macOS, `-DVTK_PYTHON_OPTIONAL_LINK:BOOL=ON` links wrapper modules via `Python3::Module` (leaving `libpython` symbols to be resolved by the loading interpreter rather than overlinking `libpython3.X.so` / `.dylib`). On Windows, `-DVTK_PYTHON_OPTIONAL_LINK:BOOL=OFF` links `python3XX.lib` directly as required by PE/COFF.

### 4. Flat Class Index (`cvista/_class_index.py`)

`cvista` supports lazy top-level class resolution (`from cvista import vtkPolyData, numpy_to_vtk`). Both `build.sh` and `bld.bat` ensure `cvista/_class_index.py` is generated and verified in `${SP_DIR}/cvista` by invoking `cvista.generate_index` across the dependency-ordered module list in `cvista/all.py`.

### 5. Platform Rendering Backends

| Platform | CMake Rendering Configuration |
|---|---|
| **Linux** (`linux-64`, `linux-aarch64`) | `VTK_USE_X=ON`, `VTK_OPENGL_HAS_EGL=ON`, `VTK_OPENGL_HAS_OSMESA=ON`, `VTK_USE_COCOA=OFF` |
| **macOS** (`osx-64`, `osx-arm64`) | `VTK_USE_COCOA=ON`, `VTK_USE_X=OFF`, `VTK_OPENGL_HAS_EGL=OFF`, `VTK_OPENGL_HAS_OSMESA=OFF` |
| **Windows** (`win-64`) | `VTK_USE_WIN32_OPENGL=ON`, `VTK_USE_COCOA=OFF`, `VTK_USE_X=OFF`, `VTK_OPENGL_HAS_EGL=OFF`, `VTK_OPENGL_HAS_OSMESA=OFF` |

### 6. Parity Contract on Conda-Forge

`cvista`'s `maxULP = 0` byte-exact contract is defined against the stock PyPI `vtk` wheel built with `cvista`'s pinned manylinux/wheel toolchain and vendored libraries. Because the conda-forge package is compiled with conda-forge's compilers, flags, and unvendored system library versions, **the conda-forge package is a functional drop-in rather than a `maxULP = 0` byte-exact match to the PyPI wheel**. It should be validated against PyVista's functional test suite and `cvista`'s relaxed-order checks.

---

## Local Verification (`rattler-build` / `conda-build`)

### Testing against a local `cvista` checkout

To test the recipe against your local checkout before a release tag tarball is published on GitHub, temporarily replace the `source:` block in `recipe/recipe.yaml` (or `recipe/meta.yaml`) with a local path:

```yaml
source:
  path: ../../..
```

#### Option A: Using `rattler-build` (Recommended, matches `conda-forge/staged-recipes` v1)

```bash
rattler-build build \
  --recipe packaging/conda-forge/recipe/recipe.yaml \
  --variant-config packaging/conda-forge/recipe/conda_build_config.yaml \
  -c conda-forge
```

#### Option B: Using `conda-build`

```bash
conda build packaging/conda-forge/recipe \
  -m packaging/conda-forge/recipe/conda_build_config.yaml \
  -c conda-forge
```

---

## Submitting to `conda-forge/staged-recipes`

1. Cut and push the `9.7.0.N` release tag on `pyvista/cvista` and compute the source archive SHA-256:
   ```bash
   VERSION="9.7.0.0"
   curl -sL "https://github.com/pyvista/cvista/archive/refs/tags/${VERSION}.tar.gz" | sha256sum
   ```
2. Update `sha256` (and `cvista_patch` if `N > 0`) in `packaging/conda-forge/recipe/recipe.yaml` and `packaging/conda-forge/recipe/meta.yaml`.
3. Fork [`conda-forge/staged-recipes`](https://github.com/conda-forge/staged-recipes) and copy the recipe directory:
   ```bash
   cp -r packaging/conda-forge/recipe /path/to/staged-recipes/recipes/cvista
   # Keep either recipe.yaml (preferred v1 format) or meta.yaml in staged-recipes/recipes/cvista/
   rm /path/to/staged-recipes/recipes/cvista/meta.yaml
   ```
4. Open a pull request against `conda-forge/staged-recipes` and verify `linux-64`, `osx-64`, `osx-arm64`, and `win-64` CI builds pass.
5. Once merged into `staged-recipes`, enable `linux_aarch64: default` and `osx_arm64: default` in `conda-forge.yml` on the newly created `conda-forge/cvista-feedstock` repository.
