// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkConeSource.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkPartitionedDataSetCollectionSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTesting.h"
#include "vtkTrackballMoveActor.h"

#include <cstdlib>
#include <iostream>

namespace
{
class MoveActorObserver : public vtkCommand
{
public:
  vtkActor* ActiveActor;
  static MoveActorObserver* New() { return new MoveActorObserver; }
  vtkTypeMacro(MoveActorObserver, vtkCommand);

  void Execute(vtkObject* vtkNotUsed(caller), unsigned long eventId, void* callData) override
  {
    switch (eventId)
    {
      case vtkTrackballMoveActor::MoveActorEvents::ApplyActiveSourcePositionEvent:
        if (this->ActiveActor)
        {
          double* position = static_cast<double*>(callData);
          this->ActiveActor->SetPosition(position);
        }
        break;
      case vtkTrackballMoveActor::MoveActorEvents::RequestActiveSourcePositionEvent:
        if (this->ActiveActor)
        {
          double* position = static_cast<double*>(callData);
          this->ActiveActor->GetPosition(position);
        }
        break;
      case vtkTrackballMoveActor::MoveActorEvents::RequestActiveSourceBoundsEvent:
        if (this->ActiveActor)
        {
          double* bounds = static_cast<double*>(callData);
          this->ActiveActor->GetBounds(bounds);
        }
        break;
      default:
        break;
    }
  }
};
}

int TestTrackballMoveActor(int argc, char* argv[])
{
  vtkNew<vtkPartitionedDataSetCollectionSource> source;
  source->SetNumberOfShapes(1);
  vtkNew<vtkCompositePolyDataMapper> mapper;
  mapper->SetInputConnection(source->GetOutputPort());

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);

  vtkNew<vtkConeSource> coneSource;
  vtkNew<vtkPolyDataMapper> coneMapper;
  coneMapper->SetInputConnection(coneSource->GetOutputPort());
  vtkNew<vtkActor> coneActor;
  coneActor->SetMapper(coneMapper);
  coneActor->SetPosition(1.0, 0.0, -1.0);

  vtkNew<vtkRenderer> renderer;
  renderer->AddActor(actor);
  renderer->AddActor(coneActor);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(300, 300);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);

  vtkNew<vtkInteractorStyleManipulator> style;
  vtkNew<vtkTrackballMoveActor> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  vtkNew<MoveActorObserver> observer;
  observer->ActiveActor = actor;

  manipulator->AddObserver(
    vtkTrackballMoveActor::MoveActorEvents::ApplyActiveSourcePositionEvent, observer);
  manipulator->AddObserver(
    vtkTrackballMoveActor::MoveActorEvents::RequestActiveSourcePositionEvent, observer);
  manipulator->AddObserver(
    vtkTrackballMoveActor::MoveActorEvents::RequestActiveSourceBoundsEvent, observer);

  renderWindow->Render();
  interactor->SetEventInformation(150, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 150; i >= 0; i -= 10)
  {
    interactor->SetEventInformation(i, 150, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  bool success = true;
  double expectedPosition[3] = { -1.87671, 0.0, 0.0 };
  double actualPosition[3];
  actor->GetPosition(actualPosition);
  for (int i = 0; i < 3; i++)
  {
    if (std::abs(actualPosition[i] - expectedPosition[i]) > 1e-5)
    {
      std::cerr << "Mismatch in position at index " << i << ": expected " << expectedPosition[i]
                << ", got " << actualPosition[i] << '\n';
      success = false;
    }
  }
  vtkNew<vtkTesting> testing;
  testing->AddArguments(argc, argv);
  if (testing->IsInteractiveModeSpecified())
  {
    interactor->Start();
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
