// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkInterpolatingSubdivisionFilter.h"

#include "cvistaCellConnectivity.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkEdgeTable.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"

// Construct object with number of subdivisions set to 1.
VTK_ABI_NAMESPACE_BEGIN
vtkInterpolatingSubdivisionFilter::vtkInterpolatingSubdivisionFilter() = default;

int vtkInterpolatingSubdivisionFilter::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (!this->Superclass::RequestData(request, inputVector, outputVector))
  {
    return 0;
  }

  // get the info objects
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // get the input and output
  vtkPolyData* input = vtkPolyData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData* output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkIdType numCells;
  int level;
  vtkPoints* outputPts;
  vtkCellArray* outputPolys;
  vtkPointData* outputPD;
  vtkCellData* outputCD;
  vtkIntArray* edgeData;

  //
  // Initialize and check input
  //

  vtkPolyData* inputDS = vtkPolyData::New();
  inputDS->CopyStructure(input);
  inputDS->GetPointData()->PassData(input->GetPointData());
  inputDS->GetCellData()->PassData(input->GetCellData());

  for (level = 0; level < this->NumberOfSubdivisions; level++)
  {
    if (this->CheckAbort())
    {
      break;
    }
    // Generate topology for the input dataset
    inputDS->BuildLinks();
    numCells = inputDS->GetNumberOfCells();

    // Copy points from input. The new points will include the old points
    // and points calculated by the subdivision algorithm
    outputPts = vtkPoints::New();
    outputPts->DeepCopy(inputDS->GetPoints());

    // Copy pointdata structure from input
    outputPD = vtkPointData::New();
    outputPD->CopyAllocate(inputDS->GetPointData(), 2 * inputDS->GetNumberOfPoints());

    // Copy celldata structure from input
    outputCD = vtkCellData::New();
    outputCD->CopyAllocate(inputDS->GetCellData(), 4 * numCells);

    // Create triangles
    outputPolys = vtkCellArray::New();
    // Output is uniformly triangles; declare fixed-size storage up front so the
    // redundant offsets array is never materialized.
    outputPolys->UseFixedSizeDefaultStorage(3);
    outputPolys->AllocateEstimate(4 * numCells, 3);

    // Create an array to hold new location indices
    edgeData = vtkIntArray::New();
    edgeData->SetNumberOfComponents(3);
    edgeData->SetNumberOfTuples(numCells);

    if (this->GenerateSubdivisionPoints(inputDS, edgeData, outputPts, outputPD) == 0)
    {
      outputPts->Delete();
      outputPD->Delete();
      outputCD->Delete();
      outputPolys->Delete();
      inputDS->Delete();
      edgeData->Delete();
      vtkErrorMacro("Subdivision failed.");
      return 0;
    }
    this->GenerateSubdivisionCells(inputDS, edgeData, outputPolys, outputCD);

    // start the next iteration with the input set to the output we just created
    edgeData->Delete();
    inputDS->Delete();
    inputDS = vtkPolyData::New();
    inputDS->SetPoints(outputPts);
    outputPts->Delete();
    inputDS->SetPolys(outputPolys);
    outputPolys->Delete();
    inputDS->GetPointData()->PassData(outputPD);
    outputPD->Delete();
    inputDS->GetCellData()->PassData(outputCD);
    outputCD->Delete();
    inputDS->Squeeze();
  } // each level

  output->SetPoints(inputDS->GetPoints());
  output->SetPolys(inputDS->GetPolys());
  output->GetPointData()->PassData(inputDS->GetPointData());
  output->GetCellData()->PassData(inputDS->GetCellData());
  inputDS->Delete();

  return 1;
}

int vtkInterpolatingSubdivisionFilter::FindEdge(vtkPolyData* mesh, vtkIdType cellId, vtkIdType p1,
  vtkIdType p2, vtkIntArray* edgeData, vtkIdList* cellIds)

{
  int edgeId = 0;
  int currentCellId = 0;
  int i;
  int numEdges;
  vtkIdType tp1, tp2;
  vtkCell* cell;

  // get all the cells that use the edge (except for cellId)
  mesh->GetCellEdgeNeighbors(cellId, p1, p2, cellIds);

  // find the edge that has the point we are looking for
  for (i = 0; i < cellIds->GetNumberOfIds(); i++)
  {
    currentCellId = cellIds->GetId(i);
    cell = mesh->GetCell(currentCellId);
    numEdges = cell->GetNumberOfEdges();
    tp1 = cell->GetPointId(2);
    tp2 = cell->GetPointId(0);
    for (edgeId = 0; edgeId < numEdges; edgeId++)
    {
      if ((tp1 == p1 && tp2 == p2) || (tp2 == p1 && tp1 == p2))
      {
        // found the edge, return the stored value
        return (int)edgeData->GetComponent(currentCellId, edgeId);
      }
      tp1 = tp2;
      tp2 = cell->GetPointId(edgeId + 1);
    }
  }
  vtkErrorMacro("Edge should have been found... but couldn't find it!!");
  return 0;
}

vtkIdType vtkInterpolatingSubdivisionFilter::InterpolatePosition(
  vtkPoints* inputPts, vtkPoints* outputPts, vtkIdList* stencil, double* weights)
{
  double xx[3], x[3];
  int i, j;

  for (j = 0; j < 3; j++)
  {
    x[j] = 0.0;
  }

  for (i = 0; i < stencil->GetNumberOfIds(); i++)
  {
    inputPts->GetPoint(stencil->GetId(i), xx);
    for (j = 0; j < 3; j++)
    {
      x[j] += xx[j] * weights[i];
    }
  }
  return outputPts->InsertNextPoint(x);
}

void vtkInterpolatingSubdivisionFilter::GenerateSubdivisionCells(
  vtkPolyData* inputDS, vtkIntArray* edgeData, vtkCellArray* outputPolys, vtkCellData* outputCD)
{
  vtkIdType numCells = inputDS->GetNumberOfCells();
  vtkIdType cellId, newId;
  int id;
  vtkIdType npts;
  const vtkIdType* pts;
  double edgePts[3];
  vtkIdType newCellPts[3];
  vtkCellData* inputCD = inputDS->GetCellData();

  // Read triangle point ids straight from native (int32) storage instead of the
  // widening GetCellPoints accessor (see cvistaCellConnectivity.h). The view
  // addresses the polys array by local cell id, which equals the dataset-global
  // cell id used below only when no verts or lines precede the polys. With
  // CheckForTriangles on (the default) the base class rejects any non-triangle
  // mesh, so that always holds; guard on it anyway so a mixed mesh
  // (CheckForTriangles off) falls back to the classic accessor and stays
  // bit-identical. This is a read-only pass, so the captured pointers stay valid.
  const cvistaCellConnectivity conn(inputDS->GetPolys());
  const bool nativeOk =
    conn.IsValid() && inputDS->GetNumberOfVerts() == 0 && inputDS->GetNumberOfLines() == 0;

  // Now create new cells from existing points and generated edge points
  for (cellId = 0; cellId < numCells; cellId++)
  {
    if (inputDS->GetCellType(cellId) != VTK_TRIANGLE)
    {
      continue;
    }
    // get the original point ids and the ids stored as cell data
    vtkIdType cellPtIds[3];
    if (nativeOk)
    {
      const vtkIdType cbeg = conn.CellBegin(cellId);
      cellPtIds[0] = conn[cbeg];
      cellPtIds[1] = conn[cbeg + 1];
      cellPtIds[2] = conn[cbeg + 2];
    }
    else
    {
      inputDS->GetCellPoints(cellId, npts, pts);
      cellPtIds[0] = pts[0];
      cellPtIds[1] = pts[1];
      cellPtIds[2] = pts[2];
    }
    edgeData->GetTuple(cellId, edgePts);

    id = 0;
    newCellPts[id++] = cellPtIds[0];
    newCellPts[id++] = (int)edgePts[1];
    newCellPts[id] = (int)edgePts[0];
    newId = outputPolys->InsertNextCell(3, newCellPts);
    outputCD->CopyData(inputCD, cellId, newId);

    id = 0;
    newCellPts[id++] = (int)edgePts[1];
    newCellPts[id++] = cellPtIds[1];
    newCellPts[id] = (int)edgePts[2];
    newId = outputPolys->InsertNextCell(3, newCellPts);
    outputCD->CopyData(inputCD, cellId, newId);

    id = 0;
    newCellPts[id++] = (int)edgePts[2];
    newCellPts[id++] = cellPtIds[2];
    newCellPts[id] = (int)edgePts[0];
    newId = outputPolys->InsertNextCell(3, newCellPts);
    outputCD->CopyData(inputCD, cellId, newId);

    id = 0;
    newCellPts[id++] = (int)edgePts[1];
    newCellPts[id++] = (int)edgePts[2];
    newCellPts[id] = (int)edgePts[0];
    newId = outputPolys->InsertNextCell(3, newCellPts);
    outputCD->CopyData(inputCD, cellId, newId);
  }
}

void vtkInterpolatingSubdivisionFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
