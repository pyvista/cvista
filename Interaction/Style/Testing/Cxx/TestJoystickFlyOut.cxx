// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkJoystickFlyOut.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSetCollectionSource.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTesting.h"

#include "InteractorStyleTestUtils.h"

#include <cstdlib>
#include <iostream>

int TestJoystickFlyOut(int argc, char* argv[])
{
  vtkNew<vtkPartitionedDataSetCollectionSource> source;
  source->SetNumberOfShapes(1);
  vtkNew<vtkCompositePolyDataMapper> mapper;
  mapper->SetInputConnection(source->GetOutputPort());

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);

  vtkNew<vtkRenderer> renderer;
  renderer->AddActor(actor);
  renderer->SetBackground(0.2, 0.3, 0.4);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(300, 300);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);

  vtkNew<vtkInteractorStyleManipulator> style;
  vtkNew<vtkJoystickFlyOut> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  renderWindow->Render();

  constexpr int maxFramesBeforeFlyOutCompletes = 250;
  vtkNew<vtkJoystickFlyObserver> observer;
  manipulator->AddObserver(vtkJoystickFly::FlyAnimationStepEvent, observer);
  observer->ReleaseLeftButtonAfter(maxFramesBeforeFlyOutCompletes);
  interactor->SetEventInformation(150, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  interactor->SetEventInformation(150, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);

  // We cannot do image comparison testing here because the amount of movement
  // depends entirely on frame rate, which is generally lower in CI than on a developer's machine,
  // due to other tests running concurrently.
  // It is also not feasible to test the camera parameters after the fly-out, because they vary
  // based on frame rate as well. Instead, we just run the fly-out and ensure that it completes
  // without crashing. The helper will automatically send a release event after 200 ticks, which
  // should be sufficient for the fly-out to complete.
  bool success = true;
  if (observer->GetFramesSinceLeftButtonDown() > maxFramesBeforeFlyOutCompletes)
  {
    std::cerr << "Fly-out did not complete within expected number of frames." << std::endl;
    success = false;
  }
  vtkNew<vtkTesting> testing;
  testing->AddArguments(argc, argv);
  if (testing->IsInteractiveModeSpecified())
  {
    observer->ReleaseLeftButtonAfter(-1);
    interactor->Start();
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
