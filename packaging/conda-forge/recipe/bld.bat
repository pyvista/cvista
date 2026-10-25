@echo off
setlocal EnableDelayedExpansion

REM Conda-forge Windows (win-64, MSVC + Win32 WGL) build script for cvista 9.7.0.x.
REM
REM Drives CMake + Ninja against cvista-config/minimal.cmake with unvendored
REM conda-forge ThirdParty libraries (VTK_USE_EXTERNAL=ON), installs the
REM top-level `cvista` Python package into %SP_DIR%\cvista and `vtk*-cvista.dll`
REM shared libraries into %LIBRARY_BIN%, generates `cvista\_class_index.py` via
REM `cvista/generate_index.py`, and writes `.dist-info` / `.egg-info` metadata.

if "%SRC_DIR%"=="" (
    for %%I in ("%~dp0..\..\..") do set "SRC_DIR=%%~fI"
)
if "%BUILD_DIR%"=="" set "BUILD_DIR=%SRC_DIR%\build-conda"
if "%PYTHON%"=="" set "PYTHON=%PREFIX%\python.exe"
if "%LIBRARY_PREFIX%"=="" set "LIBRARY_PREFIX=%PREFIX%\Library"
if "%SP_DIR%"=="" set "SP_DIR=%PREFIX%\Lib\site-packages"
if "%PKG_VERSION%"=="" set "PKG_VERSION=9.7.0.0"
if "%CPU_COUNT%"=="" set "CPU_COUNT=4"

set "PYTHON_MAJOR_VERSION=%PY_VER:~0,1%"
if "%PYTHON_MAJOR_VERSION%"=="" set "PYTHON_MAJOR_VERSION=3"

set "VTK_BASE_VERSION=9.7.0"
if "%PKG_VERSION%"=="%VTK_BASE_VERSION%" (
    set "VTK_VERSION_SUFFIX="
) else (
    set "VTK_VERSION_SUFFIX=%PKG_VERSION:9.7.0.=%"
)

set "SETUPTOOLS_SCM_PRETEND_VERSION=%PKG_VERSION%"
set "SETUPTOOLS_SCM_PRETEND_VERSION_FOR_CVISTA=%PKG_VERSION%"

REM Disable whole-program optimization (/GL + /LTCG) by default so the conda-forge
REM Windows build links cleanly against external conda DLLs within CI limits.
if "%CVISTA_LTO%"=="" set "CVISTA_LTO=0"
set "CVISTA_FORCE_MSVC=ON"
set "CXXFLAGS=-MD"
set "CFLAGS=-MD"

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
if errorlevel 1 exit /b 1

set "SRC_DIR_FWD=%SRC_DIR:\=/%"
set "BUILD_DIR_FWD=%BUILD_DIR:\=/%"
set "PREFIX_FWD=%PREFIX:\=/%"
set "LIBRARY_PREFIX_FWD=%LIBRARY_PREFIX:\=/%"
set "PYTHON_FWD=%PYTHON:\=/%"

(
    echo set^(CVISTA_FORCE_MSVC ON CACHE BOOL "cvista: take the MSVC toolchain-lever path"^)
    echo set^(CMAKE_C_FLAGS "%CFLAGS%" CACHE STRING ""^)
    echo set^(CMAKE_CXX_FLAGS "%CXXFLAGS%" CACHE STRING ""^)
    echo include^("%SRC_DIR_FWD%/cvista-config/minimal.cmake"^)
    echo list^(REMOVE_ITEM CVISTA_NOCOMPILE_CLASSES vtkCameraNode vtkLightNode vtkDiscretizableColorTransferFunction^)
    echo set^(CVISTA_NOCOMPILE_CLASSES "${CVISTA_NOCOMPILE_CLASSES}" CACHE INTERNAL "cvista: classes dropped from the C++ build" FORCE^)
) > "%BUILD_DIR%\conda-init.cmake"
if errorlevel 1 exit /b 1

REM Configure with VTK_PYTHON_OPTIONAL_LINK=OFF (Windows PE/COFF .pyd/.dll link
REM python3XX.lib directly) and CVISTA_CLASS_INDEX=OFF during CMake build so that
REM generate_index.py runs post-install under %PYTHON% where all %LIBRARY_BIN%
REM external conda DLLs are on the interpreter DLL search path.
cmake -S "%SRC_DIR_FWD%" -B "%BUILD_DIR_FWD%" -G "Ninja" ^
    %CMAKE_ARGS% ^
    -C "%BUILD_DIR_FWD%/conda-init.cmake" ^
    -Wno-dev ^
    -DCMAKE_BUILD_TYPE:STRING=Release ^
    -DCMAKE_C_COMPILER:STRING=cl ^
    -DCMAKE_CXX_COMPILER:STRING=cl ^
    -DCMAKE_PREFIX_PATH:PATH="%LIBRARY_PREFIX_FWD%" ^
    -DCMAKE_INSTALL_PREFIX:PATH="%PREFIX_FWD%" ^
    -DCMAKE_INSTALL_BINDIR:STRING="Library/bin" ^
    -DCMAKE_INSTALL_LIBDIR:STRING="Library/lib" ^
    -DCMAKE_INSTALL_INCLUDEDIR:STRING="Library/include/cvista" ^
    -DCMAKE_INSTALL_DATAROOTDIR:STRING="Library/share" ^
    -DCMAKE_INSTALL_LICENSEDIR:STRING="Library/share/licenses/cvista" ^
    -DBUILD_SHARED_LIBS:BOOL=ON ^
    -DVTK_WHEEL_BUILD:BOOL=OFF ^
    -DCVISTA_ABI3:BOOL=OFF ^
    -DCVISTA_CLASS_INDEX:BOOL=OFF ^
    -DVTK_WRAP_PYTHON:BOOL=ON ^
    -DVTK_PYTHON_VERSION:STRING="%PYTHON_MAJOR_VERSION%" ^
    -DPython3_FIND_STRATEGY:STRING=LOCATION ^
    -DPython3_ROOT_DIR:PATH="%PREFIX_FWD%" ^
    -DPython3_EXECUTABLE:FILEPATH="%PYTHON_FWD%" ^
    -DVTK_PYTHON_OPTIONAL_LINK:BOOL=OFF ^
    -DVTK_PYTHON_SITE_PACKAGES_SUFFIX:STRING="Lib/site-packages" ^
    -DVTK_CUSTOM_LIBRARY_SUFFIX:STRING=cvista ^
    -DVTK_VERSION_SUFFIX:STRING="%VTK_VERSION_SUFFIX%" ^
    -DVTK_INSTALL_PYTHON_EXES:BOOL=OFF ^
    -DVTK_INSTALL_SDK:BOOL=OFF ^
    -Dvtk_cmake_destination:STRING="Library/lib/cmake/cvista" ^
    -Dvtk_hierarchy_destination_args:STRING="HIERARCHY_DESTINATION;Library/lib/cvista/hierarchy/VTK" ^
    -DVTK_BUILD_PYI_FILES:BOOL=OFF ^
    -DVTK_BUILD_TESTING:BOOL=OFF ^
    -DVTK_BUILD_DOCUMENTATION:BOOL=OFF ^
    -DVTK_BUILD_EXAMPLES:BOOL=OFF ^
    -DVTK_WRAP_SERIALIZATION:BOOL=OFF ^
    -DVTK_HAS_FEENABLEEXCEPT:BOOL=OFF ^
    -DVTK_USE_WIN32_OPENGL:BOOL=ON ^
    -DVTK_USE_COCOA:BOOL=OFF ^
    -DVTK_USE_X:BOOL=OFF ^
    -DVTK_OPENGL_HAS_EGL:BOOL=OFF ^
    -DVTK_OPENGL_HAS_OSMESA:BOOL=OFF ^
    -DVTK_DEFAULT_RENDER_WINDOW_OFFSCREEN:BOOL=OFF ^
    -DVTK_DEFAULT_RENDER_WINDOW_HEADLESS:BOOL=OFF ^
    -DVTK_MODULE_ENABLE_VTK_RenderingExternal:STRING=NO ^
    -DVTK_MODULE_ENABLE_VTK_GUISupportMFC:STRING=NO ^
    -DVTK_USE_EXTERNAL:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_zlib:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_png:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_jpeg:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_tiff:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_freetype:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_expat:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_lz4:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_lzma:BOOL=ON ^
    -DLZMA_LIBRARY:FILEPATH="%LIBRARY_PREFIX_FWD%/lib/lzma.lib" ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_pugixml:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_jsoncpp:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_nlohmannjson:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_fmt:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_fast_float:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_eigen:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_utf8:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_hdf5:BOOL=ON ^
    -DHDF5_IS_PARALLEL:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_netcdf:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_libxml2:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_gl2ps:BOOL=ON ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_cgns:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_exprtk:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_pegtl:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_scn:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_token:BOOL=OFF ^
    -DVTK_MODULE_USE_EXTERNAL_VTK_verdict:BOOL=OFF
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR_FWD%" --parallel %CPU_COUNT%
if errorlevel 1 exit /b 1

cmake --install "%BUILD_DIR_FWD%"
if errorlevel 1 exit /b 1

REM Prune dead UI helper subpackages (gtk, tk, wx, test) from the installed
REM cvista package, keeping cvista\qt for pyvistaqt compatibility (gh-142).
for %%S in (gtk tk wx test) do (
    if exist "%SP_DIR%\cvista\%%S" rmdir /s /q "%SP_DIR%\cvista\%%S"
)

REM Generate cvista\_class_index.py (flat lazy namespace `from cvista import vtkPolyData`)
REM now that both vtk*-cvista.dll and external conda DLLs reside in %LIBRARY_BIN%.
"%PYTHON%" -c "import pathlib, re, sys, cvista, cvista.generate_index; pkg_dir = pathlib.Path(cvista.__file__).resolve().parent; all_py = (pkg_dir / 'all.py').read_text(encoding='utf-8'); modules = re.findall(r'^from \.(vtk\w+) import \*', all_py, flags=re.MULTILINE); assert modules, 'No wrapped VTK modules found in all.py'; rc = cvista.generate_index.main(['-p', 'cvista', '-o', str(pkg_dir), *modules]); assert (pkg_dir / '_class_index.py').is_file(); sys.exit(rc)"
if errorlevel 1 exit /b 1

REM Write PEP 376 .dist-info and legacy .egg-info metadata so `pip list`,
REM `pip check`, and `importlib.metadata.version("cvista")` discover the conda
REM package just like a wheel install.
set "DIST_INFO=%SP_DIR%\cvista-%PKG_VERSION%.dist-info"
if exist "%DIST_INFO%" rmdir /s /q "%DIST_INFO%"
mkdir "%DIST_INFO%"
if errorlevel 1 exit /b 1

set "METADATA_FILE=%DIST_INFO%\METADATA"
echo> "%METADATA_FILE%" Metadata-Version: 2.1
echo>>"%METADATA_FILE%" Name: cvista
echo>>"%METADATA_FILE%" Version: %PKG_VERSION%
echo>>"%METADATA_FILE%" Summary: Trimmed PyVista-parity fork of VTK 9.7.0 (top-level cvista namespace)
echo>>"%METADATA_FILE%" Home-page: https://github.com/pyvista/cvista
echo>>"%METADATA_FILE%" License: BSD-3-Clause
echo>>"%METADATA_FILE%" Requires-Python: >=3.11
echo>>"%METADATA_FILE%" Requires-Dist: matplotlib>=2.0.0
echo>>"%METADATA_FILE%" Requires-Dist: numpy>=1.9

echo>"%DIST_INFO%\INSTALLER" conda
echo>"%DIST_INFO%\top_level.txt" cvista
type nul > "%DIST_INFO%\RECORD"

set "EGG_INFO=%SP_DIR%\cvista-%PKG_VERSION%.egg-info"
echo> "%EGG_INFO%" Metadata-Version: 2.1
echo>>"%EGG_INFO%" Name: cvista
echo>>"%EGG_INFO%" Version: %PKG_VERSION%
echo>>"%EGG_INFO%" Summary: Trimmed PyVista-parity fork of VTK 9.7.0 (top-level cvista namespace)
echo>>"%EGG_INFO%" Platform: UNKNOWN
if errorlevel 1 exit /b 1
