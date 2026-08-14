// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkHDRReader.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSetCollectionSource.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkSkybox.h"
#include "vtkTestUtilities.h"
#include "vtkTesting.h"
#include "vtkTexture.h"
#include "vtkTrackballEnvironmentRotate.h"

#include <cstdlib>
#include <iostream>

int TestTrackballEnvironmentRotate(int argc, char* argv[])
{
  vtkNew<vtkPartitionedDataSetCollectionSource> source;
  source->SetNumberOfShapes(1);
  vtkNew<vtkCompositePolyDataMapper> mapper;
  mapper->SetInputConnection(source->GetOutputPort());

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);

  vtkNew<vtkRenderer> renderer;
  renderer->AddActor(actor);

  vtkNew<vtkHDRReader> reader;
  char* fname =
    vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/spiaggia_di_mondello_1k.hdr");
  reader->SetFileName(fname);
  delete[] fname;
  vtkNew<vtkTexture> texture;
  texture->SetColorModeToDirectScalars();
  texture->MipmapOn();
  texture->InterpolateOn();
  texture->SetInputConnection(reader->GetOutputPort());

  renderer->UseImageBasedLightingOn();
  renderer->SetEnvironmentTexture(texture);

  vtkNew<vtkSkybox> skybox;
  skybox->SetFloorRight(0.0, 0.0, 1.0);
  skybox->SetProjection(vtkSkybox::Sphere);
  skybox->SetTexture(texture);
  renderer->AddActor(skybox);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(300, 300);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);

  vtkNew<vtkInteractorStyleManipulator> style;
  vtkNew<vtkTrackballEnvironmentRotate> manipulator;
  style->AddManipulator(manipulator);
  interactor->SetInteractorStyle(style);

  renderWindow->Render();
  interactor->SetEventInformation(150, 150, 0, 0, 0, 0);
  interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
  for (int i = 150; i >= 0; i--)
  {
    interactor->SetEventInformation(i, 150, 0, 0, 0, 0);
    interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
  }
  interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  renderWindow->Render();

  bool success = true;
  vtkNew<vtkMatrix3x3> expectedRotation;
  expectedRotation->Identity();
  // clang-format off
  // renderer->GetEnvironmentRotationMatrix()->Print(std::cout); --- UNCOMMENT TO UPDATE EXPECTED VALUES ---
  // 0.877583        0       -0.479426
  // 0       1       0
  // 0.479426        0       0.877583
  // clang-format on
  expectedRotation->SetElement(0, 0, 0.877583);
  expectedRotation->SetElement(0, 2, -0.479426);
  expectedRotation->SetElement(1, 1, 1);
  expectedRotation->SetElement(2, 0, 0.479426);
  expectedRotation->SetElement(2, 2, 0.877583);
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      if (std::abs(renderer->GetEnvironmentRotationMatrix()->GetElement(i, j) -
            expectedRotation->GetElement(i, j)) > 1e-5)
      {
        std::cerr << "Mismatch at (" << i << ", " << j << "): expected "
                  << expectedRotation->GetElement(i, j) << ", got "
                  << renderer->GetEnvironmentRotationMatrix()->GetElement(i, j) << '\n';
        success = false;
      }
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
