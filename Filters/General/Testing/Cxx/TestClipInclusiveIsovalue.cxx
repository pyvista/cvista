// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTableBasedClipDataSet.h>
#include <vtkUnstructuredGrid.h>

#include <iostream>

vtkSmartPointer<vtkUnstructuredGrid> ClipScalar(vtkDataSet* dataset, double value, bool inside_out)
{
  auto alg = vtkSmartPointer<vtkTableBasedClipDataSet>::New();
  alg->SetValue(value);
  alg->SetInputDataObject(dataset);
  alg->SetInsideOut(inside_out);
  alg->Update();

  return alg->GetOutput();
}

vtkSmartPointer<vtkPolyData> CreateSphere()
{
  auto sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetRadius(1.0);
  sphere->SetThetaResolution(8);
  sphere->SetPhiResolution(8);
  sphere->Update();

  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->ShallowCopy(sphere->GetOutput());

  vtkIdType numPoints = poly->GetNumberOfPoints();

  auto point_ids = vtkSmartPointer<vtkIntArray>::New();
  point_ids->SetName("point_ids");
  point_ids->SetNumberOfTuples(numPoints);

  for (vtkIdType i = 0; i < numPoints; ++i)
  {
    point_ids->SetValue(i, static_cast<int>(i));
  }

  poly->GetPointData()->AddArray(point_ids);
  poly->GetPointData()->SetScalars(point_ids);
  poly->GetPointData()->SetActiveScalars("point_ids");
  return poly;
}

int TestClipInclusiveIsovalue(int, char*[])
{
  auto poly = CreateSphere();

  double range[2];
  poly->GetPointData()->GetScalars()->GetRange(range);

  auto clipped = ClipScalar(poly, 20.0, false);
  clipped->GetPointData()->GetScalars()->GetRange(range);
  if (range[0] != 20.0 || range[1] != 49.0)
  {
    std::cerr << "Unexpected range for non-inside-out clip: (" << range[0] << ", " << range[1]
              << ")\n";
    return EXIT_FAILURE;
  }

  clipped = ClipScalar(poly, 20.0, true);
  clipped->GetPointData()->GetScalars()->GetRange(range);
  if (range[0] != 0.0 || range[1] != 20.0)
  {
    std::cerr << "Unexpected range for inside-out clip: (" << range[0] << ", " << range[1] << ")\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
