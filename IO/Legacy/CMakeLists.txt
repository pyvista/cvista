set(classes
  vtkCompositeDataReader
  vtkCompositeDataWriter
  vtkDataObjectReader
  vtkDataObjectWriter
  vtkDataReader
  vtkDataSetReader
  vtkDataSetWriter
  vtkDataWriter
  vtkGenericDataObjectReader
  vtkGenericDataObjectWriter
  vtkGraphReader
  vtkGraphWriter
  vtkLegacyCellGridReader
  vtkLegacyCellGridWriter
  vtkLegacyStatisticalModelReader
  vtkLegacyStatisticalModelWriter
  vtkPixelExtentIO
  vtkPolyDataReader
  vtkPolyDataWriter
  vtkRectilinearGridReader
  vtkRectilinearGridWriter
  vtkSimplePointsReader
  vtkSimplePointsWriter
  vtkStructuredGridReader
  vtkStructuredGridWriter
  vtkStructuredPointsReader
  vtkStructuredPointsWriter
  vtkTableReader
  vtkTableWriter
  vtkTreeReader
  vtkTreeWriter
  vtkUnstructuredGridReader
  vtkUnstructuredGridWriter)

vtk_module_add_module(VTK::IOLegacy
  CLASSES ${classes}
  # Non-wrapped: installs vtkCommunicator's data-object (de)serialization handler
  # at load time so VTK::ParallelCore need not depend on any IO module.
  SOURCES vtkCommunicatorMarshal.cxx)
vtk_add_test_mangling(VTK::IOLegacy)
