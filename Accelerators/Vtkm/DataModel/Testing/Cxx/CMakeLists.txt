# We need this in order to make External Test builds to work: TestVTKMDataSet
# includes MakeTestDataSet, however, both MakeTestDataSet.h source tree and
# installed tree has two different paths. To deal with it, we need to determine
# at configure time the Viskores dependency type and pass that information down to
# the build tool so that the source code can parametrize the include.
if(NOT Viskores_FOUND)
  set_source_files_properties(TestVTKMDataSet.cxx PROPERTIES
    COMPILE_DEFINITIONS Viskores_INCLUDED_IN_VTK)
endif()

# We are splitting the tests into two executables to workaround an issue in
# cuda. With all the tests in the same executable several tests are failing
# in cuda. We have not identified the root cause of the problem yet.
vtk_add_test_cxx(vtkAcceleratorsVTKmDataModelCxxTests tests
  TestVTKMDataSet.cxx,NO_VALID
  )

vtk_test_cxx_executable(vtkAcceleratorsVTKmDataModelCxxTests tests
  RENDERING_FACTORY
  )
