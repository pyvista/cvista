// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkActor.h"
#include "vtkFloatArray.h"
#include "vtkPlaneSource.h"
#include "vtkPointData.h"
#include "vtkRegressionTestImage.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkSurfaceLICInterface.h"
#include "vtkSurfaceLICMapper.h"
#include "vtkTesting.h"

#include <cmath>
#include <cstdlib>

int TestSurfaceLICViewports(int argc, char* argv[])
{
  vtkNew<vtkPlaneSource> planeSrc;
  planeSrc->SetResolution(4, 4);
  planeSrc->Update();

  auto* plane = planeSrc->GetOutput();
  const vtkIdType nPts = plane->GetNumberOfPoints();

  vtkNew<vtkFloatArray> vectors;
  vectors->SetNumberOfComponents(3);
  vectors->SetNumberOfTuples(nPts);
  vectors->SetName("vectors");

  for (vtkIdType i = 0; i < nPts; ++i)
  {
    double xyz[3];
    plane->GetPoint(i, xyz);
    const auto& x = xyz[0];
    const auto& y = xyz[1];
    const double r = std::sqrt(x * x + y * y);
    if (r > 1.0e-6)
    {
      vectors->SetTuple3(i, -y / r, x / r, 0);
    }
    else
    {
      vectors->SetTuple3(i, 0, 0, 0);
    }
  }
  plane->GetPointData()->SetVectors(vectors);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(600, 600);
  renderWindow->SetMultiSamples(0);

  {
    vtkNew<vtkSurfaceLICMapper> mapper;
    mapper->SetInputData(plane);
    auto* lic = mapper->GetLICInterface();
    lic->SetNumberOfSteps(40);
    lic->SetStepSize(0.35);
    lic->SetEnhancedLIC(1);
    lic->SetEnhanceContrast(0);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.08, 0.08, 0.15);
    renderer->SetViewport(0.0, 0.0, 1.0, 0.5);
    renderer->AddViewProp(actor);
    renderWindow->AddRenderer(renderer);
  }
  {
    vtkNew<vtkSurfaceLICMapper> mapper;
    mapper->SetInputData(plane);
    auto* lic = mapper->GetLICInterface();
    lic->SetNumberOfSteps(40);
    lic->SetStepSize(0.35);
    lic->SetEnhancedLIC(1);
    lic->SetEnhanceContrast(0);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.05, 0.12, 0.08);
    renderer->SetViewport(0.0, 0.5, 1.0, 1.0);
    renderer->AddViewProp(actor);
    renderWindow->AddRenderer(renderer);
  }
  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);
  int retVal = vtkRegressionTestImage(renderWindow);
  if (retVal == vtkTesting::DO_INTERACTOR)
  {
    interactor->Start();
  }
  return retVal == vtkTesting::FAILED ? EXIT_FAILURE : EXIT_SUCCESS;
}
