// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCellData.h"
#include "vtkHyperTreeGrid.h"
#include "vtkHyperTreeGridExtractGhostCells.h"
#include "vtkHyperTreeGridGenerateGlobalIds.h"
#include "vtkHyperTreeGridGhostCellsGenerator.h"
#include "vtkHyperTreeGridGradient.h"
#include "vtkHyperTreeGridRedistribute.h"
#include "vtkLogger.h"
#include "vtkMPIController.h"
#include "vtkMathUtilities.h"
#include "vtkNew.h"
#include "vtkRandomHyperTreeGridSource.h"
#include "vtkStringFormatter.h"
#include "vtkTestUtilities.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK_GRADIENT(array, tupleId, x, y, z)                                                    \
  {                                                                                                \
    double grad[3];                                                                                \
    array->GetTuple(tupleId, grad);                                                                \
    if (!vtkMathUtilities::FuzzyCompare(grad[0], x, 1e-4) ||                                       \
      !vtkMathUtilities::FuzzyCompare(grad[1], y, 1e-4) ||                                         \
      !vtkMathUtilities::FuzzyCompare(grad[2], z, 1e-4))                                           \
    {                                                                                              \
      std::cerr << "Tuple " << tupleId << " expected (" << x << ", " << y << ", " << z             \
                << ") but got (" << grad[0] << ", " << grad[1] << ", " << grad[2] << ")."          \
                << std::endl;                                                                      \
      return EXIT_FAILURE;                                                                         \
    }                                                                                              \
  }

// This tests specifically cells juxtaposing ghost cells
//------------------------------------------------------------------------------
int TestPHyperTreeGridGradient(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv);
  vtkMultiProcessController::SetGlobalController(controller);

  std::string threadName = "rank #";
  threadName += vtk::to_string(controller->GetLocalProcessId());
  vtkLogger::SetThreadName(threadName);

  int rank = controller->GetLocalProcessId();
  int nbRanks = controller->GetNumberOfProcesses();

  if (nbRanks != 2)
  {
    if (rank == 0)
    {
      std::cerr << "This test requires exactly 2 MPI processes." << std::endl;
    }
    controller->Finalize();
    return EXIT_FAILURE;
  }

  vtkNew<vtkRandomHyperTreeGridSource> source;
  source->SetDimensions(10, 10, 1);
  source->SetMaxDepth(3);
  source->SetSeed(0);
  source->SetMaskedFraction(0.0);

  vtkNew<vtkHyperTreeGridRedistribute> redistribute;
  redistribute->SetInputConnection(source->GetOutputPort());

  vtkNew<vtkHyperTreeGridGhostCellsGenerator> generator;
  generator->SetInputConnection(redistribute->GetOutputPort());

  vtkNew<vtkHyperTreeGridGradient> gradient;
  gradient->SetInputConnection(generator->GetOutputPort());
  gradient->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "Depth");

  vtkNew<vtkHyperTreeGridExtractGhostCells> extractor;
  extractor->SetInputConnection(gradient->GetOutputPort());
  extractor->Update();

  vtkHyperTreeGrid* output = vtkHyperTreeGrid::SafeDownCast(extractor->GetOutput());
  vtkDataArray* gradArray = output->GetCellData()->GetArray("Gradient");

  if (rank == 0)
  {
    CHECK_GRADIENT(gradArray, 0, 49.95, 4.05, 0.0);
    CHECK_GRADIENT(gradArray, 10, 10.8, -3.6, 0.0);
  }
  if (rank == 1)
  {
    CHECK_GRADIENT(gradArray, 0, -43.65, -4.95, 0.0);
    CHECK_GRADIENT(gradArray, 10, 21.6, 0.0, 0.0);
  }

  controller->Finalize();
  return EXIT_SUCCESS;
}
