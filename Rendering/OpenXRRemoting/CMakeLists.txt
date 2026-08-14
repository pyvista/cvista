vtk_module_find_package(PRIVATE_IF_SHARED PACKAGE OpenXR VERSION 1.0.31)
vtk_module_find_package(PRIVATE_IF_SHARED PACKAGE OpenXRRemoting)

set(classes vtkOpenXRRemotingRenderWindow)

set(nowrap_classes
  vtkOpenXRManagerD3DGraphics
  vtkOpenXRManagerRemoteConnection
  )

set(nowrap_headers XrConnectionExtensions.h)

vtk_module_add_module(VTK::RenderingOpenXRRemoting
  CLASSES ${classes}
  NOWRAP_CLASSES ${nowrap_classes}
  NOWRAP_HEADERS ${nowrap_headers}
  )

vtk_module_link(VTK::RenderingOpenXRRemoting PRIVATE OpenXR::Remoting OpenXR::openxr_loader OpenXR::headers)

vtk_add_test_mangling(VTK::RenderingOpenXRRemoting)
