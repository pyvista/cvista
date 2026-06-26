set(classes
  vtkBSplineTransform
  vtkDepthSortPolyData
  vtkDSPFilterDefinition
  vtkDSPFilterGroup
  vtkEarthSource
  vtkFacetReader
  vtkForceTime
  vtkGenerateTimeSteps
  vtkGreedyTerrainDecimation
  vtkGridTransform
  vtkImageToPolyDataFilter
  vtkImplicitModeller
  vtkPCAAnalysisFilter
  vtkProcrustesAlignmentFilter
  vtkProjectedTerrainPath
  vtkTemporalArrayOperatorFilter
  vtkTemporalDataSetCache
  vtkTemporalFractal
  vtkTemporalInterpolator
  vtkTemporalShiftScale
  vtkTemporalSnapToTimeStep
  vtkTransformToGrid
  vtkWeightedTransformFilter)

# These three filters are view/camera-dependent (they use vtkCamera / vtkRenderer
# / vtkRenderWindow from RenderingCore). RenderingCore is an OPTIONAL_DEPENDS, so
# compile + wrap them ONLY when it is present. This lets FiltersHybrid -- and every
# module that DEPENDS it (e.g. FiltersParallel) -- build in a rendering-free "core"
# configuration. When RenderingCore IS enabled (the normal full/rendering build)
# all three classes are present exactly as before, so this is a drop-in no-op:
# same classes, same module, same import paths.
if (TARGET VTK::RenderingCore)
  list(APPEND classes
    vtkAdaptiveDataSetSurfaceFilter
    vtkPolyDataSilhouette
    vtkRenderLargeImage)
endif ()

vtk_module_add_module(VTK::FiltersHybrid
  CLASSES ${classes})
vtk_add_test_mangling(VTK::FiltersHybrid)
