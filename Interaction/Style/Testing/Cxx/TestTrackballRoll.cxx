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
#include "vtkTrackballRoll.h"

#include "InteractorStyleTestUtils.h"

#include <cstdlib>

int TestTrackballRoll(int argc, char* argv[])
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
  vtkNew<vtkTrackballRoll> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  renderWindow->Render();

  bool success = true;
  auto* camera = renderer->GetActiveCamera();
  std::array<double, 3> expectedFocalPoint, expectedPosition, expectedViewUp;
  std::array<double, 3> actualFocalPoint, actualPosition, actualViewUp;

  interactor->SetEventInformation(1, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 150; i >= 0; i--)
  {
    interactor->SetEventInformation(1, i, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  expectedFocalPoint = { 0.0993055, -0.112611, 0.481953 };
  camera->GetFocalPoint(actualFocalPoint.data());
  success &= assertTuplesEqual(expectedFocalPoint, actualFocalPoint, "focal point after roll");
  expectedPosition = { 0.0993055, -0.112611, 6.27176 };
  camera->GetPosition(actualPosition.data());
  success &= assertTuplesEqual(expectedPosition, actualPosition, "position after roll");
  expectedViewUp = { 0.743291, 0.668969, 0.0 };
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
