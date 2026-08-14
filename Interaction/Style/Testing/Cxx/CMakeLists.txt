vtk_add_test_cxx(vtkInteractionStyleCxxTests tests
  NO_VALID
  TestJoystickFlyIn.cxx,NO_DATA
  TestJoystickFlyOut.cxx,NO_DATA
  TestTableTopRotate.cxx,NO_DATA
  TestTrackballEnvironmentRotate.cxx
  TestTrackballMoveActor.cxx,NO_DATA
  TestTrackballMultiRotate.cxx,NO_DATA
  TestTrackballPan.cxx,NO_DATA
  TestTrackballRoll.cxx,NO_DATA
  TestTrackballRotate.cxx,NO_DATA
  TestTrackballZoom.cxx,NO_DATA
  TestTrackballZoomToMouse.cxx,NO_DATA)
vtk_test_cxx_executable(vtkInteractionStyleCxxTests tests)
