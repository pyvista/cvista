set(classes
  vtkDataObjectMeshCache
  vtkMeshCacheRunner)

vtk_module_add_module(VTK::CommonCache
  CLASSES ${classes})
vtk_add_test_mangling(VTK::CommonCache)
