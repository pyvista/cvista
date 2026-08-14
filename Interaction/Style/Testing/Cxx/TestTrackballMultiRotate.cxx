// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSetCollectionSource.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTesting.h"
#include "vtkTexture.h"
#include "vtkTrackballMultiRotate.h"

#include "InteractorStyleTestUtils.h"

#include <cstdlib>

int TestTrackballMultiRotate(int argc, char* argv[])
{
  vtkNew<vtkPartitionedDataSetCollectionSource> source;
  source->SetNumberOfShapes(1);
  vtkNew<vtkCompositePolyDataMapper> mapper;
  mapper->SetInputConnection(source->GetOutputPort());

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);

  vtkNew<vtkRenderer> renderer;
  renderer->AddActor(actor);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(300, 300);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);

  vtkNew<vtkInteractorStyleManipulator> style;
  vtkNew<vtkTrackballMultiRotate> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  renderWindow->Render();

  bool success = true;
  auto* camera = renderer->GetActiveCamera();
  std::array<double, 3> expectedFocalPoint, expectedPosition, expectedViewUp;
  std::array<double, 3> actualFocalPoint, actualPosition, actualViewUp;

  // Rotate around center
  interactor->SetEventInformation(150, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 150; i >= 0; --i)
  {
    interactor->SetEventInformation(i, 150, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  expectedFocalPoint = { -0.150135, -0.00152028, -0.481953 };
  camera->GetFocalPoint(actualFocalPoint.data());
  assertTuplesEqual(expectedFocalPoint, actualFocalPoint, "focal point after rotate");
  expectedPosition = { -0.150135, -0.00152028, -6.27176 };
  camera->GetPosition(actualPosition.data());
  assertTuplesEqual(expectedPosition, actualPosition, "position after rotate");
  expectedViewUp = { 0.0, 1.0, 0.0 };
  camera->GetViewUp(actualViewUp.data());
  assertTuplesEqual(expectedViewUp, actualViewUp, "view up after rotate");

  // Now, roll around center
  interactor->SetEventInformation(150, 2, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 150; i >= 0; --i)
  {
    interactor->SetEventInformation(i, 2, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  expectedFocalPoint = { -0.101621, 0.110526, -0.481953 };
  camera->GetFocalPoint(actualFocalPoint.data());
  success &= assertTuplesEqual(expectedFocalPoint, actualFocalPoint, "focal point after roll");
  expectedPosition = { -0.101621, 0.110526, -6.27176 };
  camera->GetPosition(actualPosition.data());
  success &= assertTuplesEqual(expectedPosition, actualPosition, "position after roll");
  expectedViewUp = { 0.742958, 0.669338, 1.08774e-16 };
  camera->GetViewUp(actualViewUp.data());
  success &= assertTuplesEqual(expectedViewUp, actualViewUp, "view up after roll");

  vtkNew<vtkTesting> testing;
  testing->AddArguments(argc, argv);
  if (testing->IsInteractiveModeSpecified())
  {
    interactor->Start();
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
