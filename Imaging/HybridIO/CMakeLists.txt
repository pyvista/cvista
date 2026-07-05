set(classes
  vtkSliceCubes)

vtk_module_add_module(VTK::ImagingHybridIO
  CLASSES ${classes})
vtk_add_test_mangling(VTK::ImagingHybridIO)
