if (NOT DEFINED VTK_INSTALL_PYTHON_EXES)
  option(VTK_INSTALL_PYTHON_EXES "Install vtkpython and pvtkpython" ON)
  mark_as_advanced(VTK_INSTALL_PYTHON_EXES)
endif ()

# Set up rpaths
set(CMAKE_BUILD_RPATH_USE_ORIGIN 1)
if (UNIX)
  file(RELATIVE_PATH vtkpython_relpath
    "/prefix/${CMAKE_INSTALL_BINDIR}"
    "/prefix/${CMAKE_INSTALL_LIBDIR}")
  if (APPLE)
    set(vtkpython_rpath_prefix
      "@executable_path")
  else ()
    set(vtkpython_rpath_prefix
      "$ORIGIN")
  endif ()

  list(APPEND CMAKE_INSTALL_RPATH
    "${vtkpython_rpath_prefix}/${vtkpython_relpath}")
endif ()

# The interpreters are not supported in wheel builds, so skip them.
if (NOT VTK_WHEEL_BUILD)
  add_executable(vtkpython)
  target_sources(vtkpython
    PRIVATE
      vtkpython.rc
      vtkPythonAppInit.cxx)
  target_link_libraries(vtkpython
    PRIVATE
      VTK::WrappingPythonCore
      VTK::PythonInterpreter
      VTK::Python
      VTK::vtkpythonmodules
      VTK::vtksys)
  add_executable(VTK::vtkpython ALIAS vtkpython)
  if (VTK_INSTALL_PYTHON_EXES)
    install(
      TARGETS     vtkpython
      EXPORT      VTKPython
      DESTINATION "${CMAKE_INSTALL_BINDIR}")
  endif ()

  if (TARGET VTK::ParallelMPI)
    add_executable(pvtkpython)
    target_sources(pvtkpython
      PRIVATE
        vtkPythonAppInit.cxx)
    target_compile_definitions(pvtkpython
      PRIVATE
        VTK_COMPILED_USING_MPI)
    target_link_libraries(pvtkpython
      PRIVATE
        VTK::WrappingPythonCore
        VTK::PythonInterpreter
        VTK::ParallelMPI
        VTK::Python
        VTK::mpi
        VTK::vtkpythonmodules)
    add_executable(VTK::pvtkpython ALIAS pvtkpython)
    if (VTK_INSTALL_PYTHON_EXES)
      install(
        TARGETS     pvtkpython
        EXPORT      VTKPython
        DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif ()
  endif ()
endif ()

# goes in __init__.py for when cvista is BUILD_STATIC
set(_cvista_static_import "_load_cvista_static()")
if (BUILD_SHARED_LIBS)
    string(PREPEND _cvista_static_import "#")
endif ()

set(_cvista_all)
set(_vtk_python_imports)
set(_vtk_python_external_imports)
# Ordered wrapped-module names + their Python targets, consumed by the flat
# class-index generation below.  The order of `vtk_python_wrapped_modules` is
# the dependency order all.py star-imports in, and the index generator must use
# the SAME order so its collision resolution matches `from cvista.all import *`.
set(_cvista_index_modules)
set(_cvista_index_module_targets)
foreach (_vtk_python_module IN LISTS vtk_python_wrapped_modules)
  get_property(_vtk_python_library_name
    TARGET    "${_vtk_python_module}"
    PROPERTY  "INTERFACE_vtk_module_library_name")
  string(APPEND _cvista_all
    "    \'${_vtk_python_library_name}\',\n")
  string(APPEND _vtk_python_imports
    "from .${_vtk_python_library_name} import *\n")
  string(APPEND _vtk_python_external_imports
    "from cvista.${_vtk_python_library_name} import *\n")
  list(APPEND _cvista_index_modules "${_vtk_python_library_name}")
  if (TARGET "${_vtk_python_library_name}Python")
    list(APPEND _cvista_index_module_targets "${_vtk_python_library_name}Python")
  endif ()
endforeach ()

set(_vtk_python_files)
foreach (_vtk_module IN LISTS vtk_modules)
  _vtk_module_get_module_property("${_vtk_module}"
    PROPERTY "python_modules"
    VARIABLE _vtk_module_python_files)
  list(APPEND _vtk_python_files
    ${_vtk_module_python_files})
endforeach()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cvista/generate_pyi.py"
  "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_pyi.py"
  COPYONLY)
list(APPEND _vtk_python_files
  "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_pyi.py")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cvista/generate_index.py"
  "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_index.py"
  COPYONLY)
list(APPEND _vtk_python_files
  "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_index.py")

if (BUILD_SHARED_LIBS)
  install(
    FILES       "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_pyi.py"
                "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/generate_index.py"
    DESTINATION "${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista"
    COMPONENT   "python")
endif ()

set(python_files
  cvista/__init__.py.in
  cvista/all.py.in
  cvista/gtk/GtkGLExtVTKRenderWindow.py
  cvista/gtk/GtkGLExtVTKRenderWindowInteractor.py
  cvista/gtk/GtkVTKRenderWindow.py
  cvista/gtk/GtkVTKRenderWindowInteractor.py
  cvista/gtk/__init__.py
  cvista/numpy_interface/__init__.py
  cvista/numpy_interface/_vtk_array_mixin.py
  cvista/numpy_interface/algorithms.py
  cvista/numpy_interface/array_overrides.py
  cvista/numpy_interface/dataset_adapter.py
  cvista/numpy_interface/internal_algorithms.py
  cvista/numpy_interface/numpy_algorithms.py
  cvista/numpy_interface/utils.py
  cvista/numpy_interface/vtk_affine_array.py
  cvista/numpy_interface/vtk_aos_array.py
  cvista/numpy_interface/vtk_composite_array.py
  cvista/numpy_interface/vtk_constant_array.py
  cvista/numpy_interface/vtk_implicit_array.py
  cvista/numpy_interface/vtk_indexed_array.py
  cvista/numpy_interface/vtk_none_array.py
  cvista/numpy_interface/vtk_partitioned_array.py
  cvista/numpy_interface/vtk_soa_array.py
  cvista/numpy_interface/vtk_strided_array.py
  cvista/numpy_interface/vtk_structured_point_array.py
  cvista/qt/QVTKRenderWindowInteractor.py
  cvista/qt/__init__.py
  cvista/test/BlackBox.py
  cvista/test/ErrorObserver.py
  cvista/test/Testing.py
  cvista/test/rtImageTest.py
  cvista/test/__init__.py
  cvista/tk/__init__.py
  cvista/tk/vtkLoadPythonTkWidgets.py.in
  cvista/tk/vtkTkImageViewerWidget.py
  cvista/tk/vtkTkPhotoImage.py
  cvista/tk/vtkTkRenderWidget.py
  cvista/tk/vtkTkRenderWindowInteractor.py
  cvista/util/__init__.py
  cvista/util/colors.py
  cvista/util/data_array_selection.py
  cvista/util/data_model.py
  cvista/util/execution_model.py
  cvista/util/functions.py
  cvista/util/keys.py
  cvista/util/misc.py
  cvista/util/numpy_support.py
  cvista/util/xarray_support.py
  cvista/util/pickle_support.py
  cvista/util/vtkAlgorithm.py
  cvista/util/vtkConstants.py
  cvista/util/vtkImageExportToArray.py
  cvista/util/vtkImageImportFromArray.py
  cvista/util/vtkMethodParser.py
  cvista/util/vtkVariant.py
  cvista/wx/__init__.py
  cvista/wx/wxVTKRenderWindow.py
  cvista/wx/wxVTKRenderWindowInteractor.py)


foreach (python_file IN LISTS python_files)
  # add submodules that don't begin with "_" to _cvista_all
  if (python_file MATCHES "^cvista\/([A-Za-z][A-Za-z0-9_]*)(\/__init__)?[.]py")
    string(APPEND _cvista_all "    \'${CMAKE_MATCH_1}\',\n")
  endif ()
endforeach ()

set(_vtk_python_depends_args)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
  list(APPEND _vtk_python_depends_args
    DEPENDS_EXPLICIT_ONLY)
endif ()

# used by vtkLoadPythonTkWidgets.py.in
set(_renderingtk_output_name "vtkRenderingTk")
if (TARGET VTK::RenderingTk)
  get_target_property(_renderingtk_output_name VTK::RenderingTk OUTPUT_NAME)
endif ()

set(python_copied_modules)
foreach (python_file IN LISTS python_files)
  set(output_python_file
    "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/${python_file}")
  if (python_file MATCHES "\\.in$")
    string(REPLACE ".in" "" output_python_file "${output_python_file}")
    configure_file(
      "${python_file}"
      "${output_python_file}"
      @ONLY)
  else ()
    add_custom_command(
      OUTPUT  "${output_python_file}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${python_file}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${CMAKE_CURRENT_SOURCE_DIR}/${python_file}"
              "${output_python_file}"
      COMMENT "Copying ${python_file} to the binary directory"
      ${_vtk_python_depends_args})
  endif ()
  if (BUILD_SHARED_LIBS)
    get_filename_component(python_file_directory "${python_file}" DIRECTORY)
    install(
      FILES       "${output_python_file}"
      DESTINATION "${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/${python_file_directory}"
      COMPONENT   "python")
  endif ()
  list(APPEND python_copied_modules
    "${output_python_file}")
endforeach ()

set(vtk_build_paths_module
  "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/_build_paths.py")
if (WIN32 AND VTK_DLL_PATHS)
  set(vtk_dll_paths_python)
  foreach (vtk_dll_path IN LISTS VTK_DLL_PATHS)
    file(TO_NATIVE_PATH "${vtk_dll_path}" vtk_dll_path_native)
    string(APPEND vtk_dll_paths_python
      "    r'${vtk_dll_path_native}',\n")
  endforeach ()
  file(WRITE "${vtk_build_paths_module}"
    "paths = [\n${vtk_dll_paths_python}]\n")
  # Add the file to the zip only if required in the install tree.
  if (NOT VTK_RELOCATABLE_INSTALL AND NOT VTK_UNIFIED_INSTALL_TREE)
    list(APPEND _vtk_python_files
      "${vtk_build_paths_module}")
  endif ()
elseif (EXISTS "${vtk_build_paths_module}")
  file(REMOVE "${vtk_build_paths_module}")
endif ()

list(APPEND _vtk_python_files
  ${python_copied_modules})

set(_vtk_python_zip)
if (NOT BUILD_SHARED_LIBS)
  set(_vtk_python_zip "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/_vtk.zip")
  add_custom_command(
    OUTPUT            "${_vtk_python_zip}"
    COMMAND           ${CMAKE_COMMAND} -E tar "cfv"
                      "${_vtk_python_zip}" --format=zip ${_vtk_python_files}
    DEPENDS           ${_vtk_python_files}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}"
    COMMENT           "Creating _vtk.zip Python modules archive"
    ${_vtk_python_depends_args})
  install(
    FILES       "${_vtk_python_zip}"
    DESTINATION "${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/"
    COMPONENT   "python")
endif ()

if (NOT VTK_ABI_NAMESPACE_NAME STREQUAL "<DEFAULT>" AND NOT DEFINED ENV{CI})
  message(WARNING "Wrappings::Python ABI does not not support the VTK_ABI_NAMESPACE_NAME "
                  "and the symbols will not be mangled.")
endif ()

add_custom_target(vtk_python_copy ALL
  DEPENDS
    ${python_copied_modules}
    ${_vtk_python_zip})

# ---- Flat class index (cvista extension) ------------------------------------
# Generate `cvista/_class_index.py` (public name -> hosting module) by importing
# every wrapped module of THIS build -- see cvista/generate_index.py for the
# semantics and cvista/__init__.py.in for the consuming module __getattr__.
# Mirrors the .pyi generation in vtkModuleWrapPython.cmake, but lives here
# (a cvista-owned file) so upstream-sync diffs stay clean.  Skipped for static
# builds (no shared-module import path) -- the flat namespace degrades
# gracefully when the index is absent.
option(CVISTA_CLASS_INDEX "Generate the flat class index (cvista/_class_index.py)" ON)
mark_as_advanced(CVISTA_CLASS_INDEX)

if (CVISTA_CLASS_INDEX AND BUILD_SHARED_LIBS)
  if (TARGET VTK::vtkpython)
    set(_cvista_index_python_exe "$<TARGET_FILE:VTK::vtkpython>")
  else ()
    set(_cvista_index_python_exe "${Python3_EXECUTABLE}")
  endif ()

  set(_cvista_index_file
    "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista/_class_index.py")

  add_custom_command(
    OUTPUT    "${_cvista_index_file}"
    COMMAND   ${_cvista_index_python_exe} # Do not quote; may contain arguments.
              -m cvista.generate_index
              -p "cvista"
              -o "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista"
              ${_cvista_index_modules}
    WORKING_DIRECTORY
              "${CMAKE_BINARY_DIR}/${VTK_PYTHON_SITE_PACKAGES_SUFFIX}"
    DEPENDS   ${_cvista_index_module_targets}
              "${CMAKE_CURRENT_SOURCE_DIR}/cvista/generate_index.py"
              vtk_python_copy
    COMMENT   "Generating the cvista flat class index"
    ${_vtk_python_depends_args})

  add_custom_target(cvista_class_index ALL
    DEPENDS "${_cvista_index_file}")

  install(
    FILES       "${_cvista_index_file}"
    DESTINATION "${VTK_PYTHON_SITE_PACKAGES_SUFFIX}/cvista"
    COMPONENT   "python")
endif ()

if (VTK_BUILD_TESTING)
  add_subdirectory(Testing)
endif ()
