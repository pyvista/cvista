// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkConeSource.h"
#include "vtkCullerCollection.h"
#include "vtkNew.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRegressionTestImage.h"
#include "vtkWebXRRenderWindow.h"
#include "vtkWebXRRenderWindowInteractor.h"
#include "vtkWebXRRenderer.h"

#include <emscripten/em_asm.h>
#include <emscripten/html5.h>

int TestWebXRRendering(int argc, char* argv[])
{
  // Setup IWER and test function
  // clang-format off
  EM_ASM(
    const xrDevice = new IWER.XRDevice(IWER.metaQuest3);
    xrDevice.installRuntime();
    xrDevice.position.set(0, 1.6, 2);
  , 0);
  // clang-format on

  // Create a renderer, render window, and interactor
  vtkNew<vtkWebXRRenderer> renderer;
  vtkNew<vtkWebXRRenderWindow> renderWindow;
  renderWindow->AddRenderer(renderer);
  renderWindow->SetSize(300, 300);

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

  // Prevent camera issues when no actors are visible in the viewport
  renderer->GetCullers()->RemoveAllItems();

  renderer->SetBackground(0.0231, 0.4194, 0.5592);

  renderWindowInteractor->Initialize();

  vtkWebXRRenderWindowInteractor::StartXR();

  // Wait for WebXR session to start
  emscripten_sleep(100);
  // Force canvas size to 300x300 because IWER overrides it
  emscripten_set_canvas_element_size("canvas", 300, 300);
  // Wait to let WebXR request at least one frame
  emscripten_sleep(100);

  int retVal = vtkRegressionTestImage(renderWindow);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    renderWindowInteractor->Start();
  }

  return !retVal;
}
