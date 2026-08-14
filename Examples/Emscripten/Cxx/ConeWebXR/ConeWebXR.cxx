// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkConeSource.h"
#include "vtkCullerCollection.h"
#include "vtkNew.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkWebXRRenderWindow.h"
#include "vtkWebXRRenderWindowInteractor.h"
#include "vtkWebXRRenderer.h"
#include <emscripten.h>

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  // Create a renderer, render window, and interactor
  vtkNew<vtkWebXRRenderer> renderer;
  vtkNew<vtkWebXRRenderWindow> renderWindow;
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkWebXRRenderWindowInteractor> renderWindowInteractor;
  renderWindowInteractor->SetRenderWindow(renderWindow);

  // Create pipeline
  vtkNew<vtkConeSource> coneSource;
  coneSource->SetResolution(10);
  vtkNew<vtkPolyDataMapper> coneMapper;
  coneMapper->SetInputConnection(coneSource->GetOutputPort());
  vtkNew<vtkActor> coneActor;
  coneActor->SetMapper(coneMapper);
  coneActor->SetPickable(true);
  renderer->AddActor(coneActor);

  // WebXR emulator doesn't support multisamples>1
  renderWindow->SetMultiSamples(0);
  // Set helper window size for WebXR emulator
  renderWindow->GetHelperWindow()->SetSize(900, 900);

  // Prevent camera issues when no actors are visible in the viewport
  renderer->GetCullers()->RemoveAllItems();

  renderer->SetBackground(0.0231, 0.4194, 0.5592);

  // Start rendering app
  renderWindowInteractor->Start();

  return 0;
}

extern "C"
{

  EMSCRIPTEN_KEEPALIVE
  void StartXR()
  {
    vtkWebXRRenderWindowInteractor::StartXR(vtkWebXRRenderWindowInteractor::SessionMode::VR);
  }

  EMSCRIPTEN_KEEPALIVE
  void StopXR()
  {
    vtkWebXRRenderWindowInteractor::StopXR();
  }
}
