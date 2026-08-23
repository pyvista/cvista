# cvista: the serial vtkExodusIIWriter base is trimmed from IO/Exodus, so the
# parallel writer (vtkPExodusIIWriter) and its object-factory override cannot
# build here. cvista IO is reader-scoped; ship the parallel reader only.
set(classes
  vtkPExodusIIReader)

vtk_module_add_module(VTK::IOParallelExodus
  CLASSES ${classes})
vtk_add_test_mangling(VTK::IOParallelExodus)
