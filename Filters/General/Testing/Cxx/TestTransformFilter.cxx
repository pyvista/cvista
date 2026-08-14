// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include <vtkCellData.h>
#include <vtkDataArrayRange.h>
#include <vtkFloatArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>

#include <cstdlib>
#include <iostream>

namespace
{
bool CheckVector(vtkDataArray* array, double expectedTuple[3])
{
  const double tolerance = 0.001;
  if (!array)
  {
    std::cerr << "No array found\n";
    return false;
  }
  auto norm = array->GetTuple3(0);
  if (vtkMath::Distance2BetweenPoints(norm, expectedTuple) > tolerance)
  {
    std::cerr << array->GetName() << " was not transformed correctly\n";
    for (auto v : vtk::DataArrayValueRange(array))
    {
      std::cerr << " " << v;
    }
    std::cerr << "\n";
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
void InitializePointSet(vtkPolyData* pointSet, int dataType)
{
  vtkNew<vtkPoints> points;
  points->SetDataType(dataType);

  constexpr int numPoints = 4;
  points->InsertNextPoint(0, 0, 0);
  points->InsertNextPoint(1, 0, 0);
  points->InsertNextPoint(1, 1, 0);
  points->InsertNextPoint(0, 1, 0);
  pointSet->SetPoints(points);

  vtkNew<vtkCellArray> quad;
  quad->InsertNextCell({ 0, 1, 2, 3 });
  pointSet->SetPolys(quad);

  // Add texture coordinates. Values don't matter, we just want to make sure
  // they are passed through the transform filter.
  vtkNew<vtkFloatArray> tcoords;
  tcoords->SetName("tcoords");
  tcoords->SetNumberOfComponents(2);
  tcoords->SetNumberOfTuples(numPoints);
  tcoords->FillComponent(0, 0.0);
  tcoords->FillComponent(1, 1.0);
  pointSet->GetPointData()->SetTCoords(tcoords);

  vtkNew<vtkFloatArray> vectors;
  vectors->SetName("Vectors");
  vectors->SetNumberOfComponents(3);
  vectors->InsertNextTuple3(-1, -1, 0);
  vectors->InsertNextTuple3(1, -1, 0);
  vectors->InsertNextTuple3(1, 1, 0);
  vectors->InsertNextTuple3(-1, 1, 0);
  pointSet->GetPointData()->SetVectors(vectors);

  vtkNew<vtkFloatArray> data;
  data->SetName("Data");
  data->SetNumberOfTuples(numPoints);
  data->FillValue(42);
  pointSet->GetPointData()->AddArray(data);

  vtkNew<vtkFloatArray> vel;
  vel->SetName("Vel");
  vel->SetNumberOfComponents(3);
  vel->SetNumberOfTuples(numPoints);
  vel->FillComponent(0, 0.1);
  vel->FillComponent(1, 1.1);
  vel->FillComponent(2, 2.1);
  pointSet->GetPointData()->AddArray(vel);

  vtkNew<vtkFloatArray> normals;
  normals->SetName("Normals");
  normals->SetNumberOfComponents(3);
  normals->InsertNextTuple3(0, 0, 1);
  pointSet->GetCellData()->SetNormals(normals);
}

//------------------------------------------------------------------------------
void InitializeTransform(vtkTransform* transform)
{
  transform->RotateY(90);
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkPointSet> TransformPointSet(
  int dataType, int outputPointsPrecision, bool transformVec = false)
{
  vtkNew<vtkPolyData> inputPointSet;
  InitializePointSet(inputPointSet, dataType);

  vtkNew<vtkTransform> transform;
  InitializeTransform(transform);

  vtkNew<vtkTransformFilter> transformFilter;
  transformFilter->SetTransformAllInputVectors(transformVec);
  transformFilter->SetOutputPointsPrecision(outputPointsPrecision);

  transformFilter->SetTransform(transform);
  transformFilter->SetInputData(inputPointSet);

  transformFilter->Update();

  vtkSmartPointer<vtkPointSet> outputPointSet = transformFilter->GetOutput();
  vtkSmartPointer<vtkPoints> points = outputPointSet->GetPoints();

  return outputPointSet;
}

//------------------------------------------------------------------------------
bool TestOutputPrecision()
{
  vtkSmartPointer<vtkPointSet> pointSet =
    TransformPointSet(VTK_FLOAT, vtkAlgorithm::DEFAULT_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_FLOAT)
  {
    return false;
  }

  pointSet = TransformPointSet(VTK_DOUBLE, vtkAlgorithm::DEFAULT_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_DOUBLE)
  {
    return false;
  }

  pointSet = TransformPointSet(VTK_FLOAT, vtkAlgorithm::SINGLE_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_FLOAT)
  {
    return false;
  }

  pointSet = TransformPointSet(VTK_DOUBLE, vtkAlgorithm::SINGLE_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_FLOAT)
  {
    return false;
  }

  pointSet = TransformPointSet(VTK_FLOAT, vtkAlgorithm::DOUBLE_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_DOUBLE)
  {
    return false;
  }

  pointSet = TransformPointSet(VTK_DOUBLE, vtkAlgorithm::DOUBLE_PRECISION);

  if (pointSet->GetPoints()->GetDataType() != VTK_DOUBLE)
  {
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
bool TestOutputAttributes()
{
  vtkSmartPointer<vtkPointSet> pointSet =
    TransformPointSet(VTK_FLOAT, vtkAlgorithm::DEFAULT_PRECISION);

  if (pointSet->GetPointData()->GetTCoords() == nullptr)
  {
    std::cerr << "TCoords were not passed through vtkTransformFilter." << std::endl;
    return false;
  }

  auto vectors = pointSet->GetPointData()->GetVectors();
  // Vectors are always transformed
  double expectedVec0[3] = { 0, -1, 1 };
  bool res = ::CheckVector(vectors, expectedVec0);

  auto normals = pointSet->GetCellData()->GetNormals();
  double expectedNormals0[3] = { 1, 0, 0 };
  res = res && ::CheckVector(normals, expectedNormals0);

  // other input vectors should be preserved.
  double transformedVec0[3] = { 0.1, 1.1, 2.1 };
  auto vel = pointSet->GetPointData()->GetArray("Vel");
  res = res && ::CheckVector(vel, transformedVec0);

  auto data = pointSet->GetPointData()->GetArray("Data");
  if (!data)
  {
    std::cerr << "Array Data not found\n";
    res = false;
  }

  return res;
}

//------------------------------------------------------------------------------
bool TestTransformAllVectors()
{
  auto pointSet = TransformPointSet(VTK_DOUBLE, vtkAlgorithm::DEFAULT_PRECISION, true);

  auto vectors = pointSet->GetPointData()->GetVectors();
  double expectedVec0[3] = { 0, -1, 1 };
  bool res = ::CheckVector(vectors, expectedVec0);

  auto normals = pointSet->GetCellData()->GetNormals();
  double expectedNormals0[3] = { 1, 0, 0 };
  res = res && ::CheckVector(normals, expectedNormals0);

  double transformedVec0[3] = { 2.1, 1.1, -0.1 };
  auto vec = pointSet->GetPointData()->GetArray("Vel");
  res = res && ::CheckVector(vec, transformedVec0);

  return res;
}
}

//------------------------------------------------------------------------------
int TestTransformFilter(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  if (!::TestOutputPrecision())
  {
    std::cerr << "TestOutputPrecision failed.\n";
    return EXIT_FAILURE;
  }

  if (!::TestOutputAttributes())
  {
    std::cerr << "TestOutputAttributes failed.\n";
    return EXIT_FAILURE;
  }

  if (!::TestTransformAllVectors())
  {
    std::cerr << "TestTransformAllVectors failed.\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
