# Resolve the NanoVDB external dependency
find_path(OpenVDB_CMAKE_PATH
  NAMES FindOpenVDB.cmake
  DOC "Path to OpenVDB CMake files")
mark_as_advanced(OpenVDB_CMAKE_PATH)

list(INSERT CMAKE_MODULE_PATH 0
  "${OpenVDB_CMAKE_PATH}")
vtk_module_find_package(PRIVATE_IF_SHARED
  PACKAGE OpenVDB
  COMPONENTS
    nanovdb)

set(classes
  vtkNanoVDBWriter)

vtk_module_add_module(VTK::IONanoVDB
  CLASSES ${classes})

vtk_module_link(VTK::IONanoVDB
  NO_KIT_EXPORT_IF_SHARED
  PRIVATE
    OpenVDB::nanovdb)

vtk_module_include(VTK::IONanoVDB
  PRIVATE
    ${OpenVDB_nanovdb_INCLUDE_DIR})

vtk_add_test_mangling(VTK::IONanoVDB)
