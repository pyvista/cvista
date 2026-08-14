vtk_add_test_cxx(vtkCommonCacheCxxTests tests
  TestDataObjectMeshCache.cxx, NO_VALID
  TestMeshCacheRunner.cxx, NO_VALID
)

vtk_test_cxx_executable(vtkCommonCacheCxxTests tests)
