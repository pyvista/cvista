set(_vtk_test_cxx_wasm_enabled_in_browser ON)

vtk_add_test_cxx(vtkRenderingWebXRCxxTests tests
  TestWebXRRendering.cxx NO_SERDES
  TestWebXRInteraction.cxx NO_SERDES)

vtk_test_cxx_executable(vtkRenderingWebXRCxxTests tests)

# For JavaScript Async functions (await)
target_link_options(vtkRenderingWebXRCxxTests
  PRIVATE
    "-Wno-experimental"
    "-sJSPI")
