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
#include "vtkTrackballZoomToMouse.h"

#include "InteractorStyleTestUtils.h"

#include <cstdlib>

int TestTrackballZoomToMouse(int argc, char* argv[])
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
  vtkNew<vtkTrackballZoomToMouse> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  renderWindow->Render();

  bool success = true;
  auto* camera = renderer->GetActiveCamera();
  std::array<double, 3> expectedFocalPoint, expectedPosition, expectedViewUp;
  std::array<double, 3> actualFocalPoint, actualPosition, actualViewUp;

  interactor->SetEventInformation(280, 280, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 280; i >= 180; i -= 10)
  {
    interactor->SetEventInformation(280, i, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  expectedFocalPoint = { 0.966593, 0.814938, -3.03389 };
  camera->GetFocalPoint(actualFocalPoint.data());
  success &=
    assertTuplesEqual(expectedFocalPoint, actualFocalPoint, "focal point after zoom-to-mouse");
  expectedPosition = { 0.966593, 0.814938, 2.75592 };
  camera->GetPosition(actualPosition.data());
  success &= assertTuplesEqual(expectedPosition, actualPosition, "position after zoom-to-mouse");
  expectedViewUp = { 0.0, 1.0, 0.0 };
  camera->GetViewUp(actualViewUp.data());
  success &= assertTuplesEqual(expectedViewUp, actualViewUp, "view up after zoom-to-mouse");

  vtkNew<vtkTesting> testing;
  testing->AddArguments(argc, argv);
  if (testing->IsInteractiveModeSpecified())
  {
    interactor->Start();
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
