// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "TestPolarAxesInternal.h"
#include "vtkPolarAxesActor.h"
#include "vtkRenderWindowInteractor.h"

//------------------------------------------------------------------------------
int TestPolarAxes(int argc, char* argv[])
{
  vtkNew<vtkPolarAxesActor> polarAxes;
  ::InitializeAxes(polarAxes);
  polarAxes->SetRange(0.1, 10);
  polarAxes->SetLog(true);
  vtkNew<vtkRenderWindowInteractor> interactor;
  ::CreatePolarAxesPipeline(argc, argv, polarAxes, interactor);
  interactor->Start();
  return EXIT_SUCCESS;
}
