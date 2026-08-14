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

EM_ASYNC_JS(void, start_test, (), { await moveActor(); });

int TestWebXRInteraction(int argc, char* argv[])
{
  // Setup the WebXR emulator and test function
  // clang-format off
  EM_ASM(
    const xrDevice = new IWER.XRDevice(IWER.metaQuest3);
    xrDevice.installRuntime();
    xrDevice.position.set(0, 1.6, 2);

    const sleep = (delay) => new Promise((resolve) => setTimeout(resolve, delay));

    globalThis.moveActor = async function() {
      const leftController = xrDevice.controllers.left;

      leftController.position.set(0, 0, 0.4);
      // wait for 100 ms to let the WebXR runtime render at least one frame between each
      // instruction
      await sleep(100);
      leftController.updateButtonValue('trigger', 1.0);
      await sleep(100);
      leftController.position.set(0, 2, 1);
      await sleep(100);
      leftController.updateButtonValue('trigger', 0.0);
      await sleep(100);
    }
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

  start_test();

  // Force canvas size to 300x300 because the WebXR emulator overrides it
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
