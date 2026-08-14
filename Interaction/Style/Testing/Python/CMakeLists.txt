include(vtkOpenGLOptions)
if (vtk_can_do_onscreen AND NOT VTK_DEFAULT_RENDER_WINDOW_OFFSCREEN)
  vtk_add_test_python(
    TestFlyTo.py
    TestStyleManipulator.py,NO_RT
    TestStyleRubberBandZoom.py
    TestStyleRubberBandZoomPerspective.py,NO_RT
    TestInteractorStyleTerrain.py
    TestStyleBaseSpike.py
    TestStyleJoystickActor.py
    TestStyleJoystickCamera.py
    TestStyleTerrain.py
    TestStyleTrackballActor.py
    TestStyleTrackballCamera.py
    TestStyleUser.py
    )

  set_tests_properties(VTK::InteractionStylePython-TestStyleRubberBandZoomPerspective
                       PROPERTIES RUN_SERIAL ON)
endif ()
