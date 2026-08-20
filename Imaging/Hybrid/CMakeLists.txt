set(classes
  vtkBooleanTexture
  vtkCheckerboardSplatter
  vtkFastSplatter
  vtkGaussianSplatter
  vtkImageCursor3D
  vtkImageRectilinearWipe
  vtkImageToPoints
  vtkPointLoad
  vtkSampleFunction
  vtkShepardMethod
  vtkSurfaceReconstructionFilter
  vtkTriangularTexture
  vtkVoxelModeller)

vtk_module_add_module(VTK::ImagingHybrid
  CLASSES ${classes})
vtk_add_test_mangling(VTK::ImagingHybrid)
