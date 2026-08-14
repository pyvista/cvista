// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <string>

#include "vtkClipDataSet.h"
#include "vtkDIYAggregateDataSetFilter.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkMPIController.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPartitionedDataSetCollection.h"
#include "vtkPlane.h"
#include "vtkProcessGroup.h"
#include "vtkRTAnalyticSource.h"
#include "vtkRedistributeDataSetFilter.h"
#include "vtkRedistributeDataSetToSubCommFilter.h"
#include "vtkSmartPointer.h"
#include "vtkSphereSource.h"
#include "vtkStringFormatter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkXMLPartitionedDataSetWriter.h"
#include <vtkGroupDataSetsFilter.h>
#include <vtk_mpi.h>

#include <iostream>

namespace
{

vtkMPIController* Controller = nullptr;

void LogMessage(const std::string& msg)
{
  if (Controller->GetLocalProcessId() == 0)
  {
    std::cout << msg << std::endl;
    std::cout.flush();
  }
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkPartitionedDataSet> CreatePartitionedDataSet()
{
  vtkNew<vtkPartitionedDataSet> parts;
  int partCount = 10;
  parts->SetNumberOfPartitions(partCount);
  for (int cc = 0; cc < partCount; ++cc)
  {
    vtkNew<vtkSphereSource> sphere;
    sphere->SetCenter(cc, 0, 0);
    sphere->Update();
    parts->SetPartition(cc, sphere->GetOutputDataObject(0));
  }
  return parts;
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> CreateImageData()
{
  const int nProcs = Controller->GetNumberOfProcesses();
  const int myRank = Controller->GetLocalProcessId();

  // create a wavelet source
  vtkNew<vtkRTAnalyticSource> waveletSource;
  waveletSource->SetWholeExtent(0, 58, 0, 56, 0, 50);
  waveletSource->UpdatePiece(myRank, nProcs, 0);

  // print the initial number of vertices on each partition
  std::cout << "WAVELET: rank " << myRank << " has "
            << waveletSource->GetOutput()->GetNumberOfElements(0) << "\n";

  return vtkImageData::SafeDownCast(waveletSource->GetOutput());
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkUnstructuredGrid> CreateUnstructuredGrid()
{
  const int nProcs = Controller->GetNumberOfProcesses();
  const int myRank = Controller->GetLocalProcessId();

  // create a wavelet source
  vtkNew<vtkRTAnalyticSource> waveletSource;
  waveletSource->SetWholeExtent(0, 58, 0, 56, 0, 50);
  waveletSource->UpdatePiece(myRank, nProcs, 0);

  // print the initial number of vertices on each partition
  std::cout << "WAVELET: rank " << myRank << " has "
            << waveletSource->GetOutput()->GetNumberOfElements(0) << "\n";

  // clip the corner off the box
  vtkNew<vtkClipDataSet> clipFilter;
  clipFilter->SetInputConnection(waveletSource->GetOutputPort());
  vtkNew<vtkPlane> plane;
  plane->SetOrigin(10, 10, 10);
  plane->SetNormal(-1.0, -1.0, -1.0);
  clipFilter->SetClipFunction(plane);
  clipFilter->UpdatePiece(myRank, nProcs, 0);

  // print the number of vertices on each partition after clipping
  std::cout << "CLIPPED: rank " << myRank << " has "
            << clipFilter->GetOutput()->GetNumberOfElements(0) << "\n";

  return vtkUnstructuredGrid::SafeDownCast(clipFilter->GetOutput());
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkPartitionedDataSetCollection> CreatePartitionedDatasetCollection()
{
  const int nProcs = Controller->GetNumberOfProcesses();
  const int myRank = Controller->GetLocalProcessId();

  // create a wavelet source
  vtkNew<vtkRTAnalyticSource> waveletSource;
  waveletSource->SetWholeExtent(0, 58, 0, 56, 0, 50);
  waveletSource->UpdatePiece(myRank, nProcs, 0);

  // print the initial number of vertices on each partition
  std::cout << "WAVELET: rank " << myRank << " has "
            << waveletSource->GetOutput()->GetNumberOfElements(0) << "\n";

  // clip the corner off the box
  vtkNew<vtkClipDataSet> clipFilter;
  clipFilter->SetInputConnection(waveletSource->GetOutputPort());
  vtkNew<vtkPlane> plane;
  plane->SetOrigin(2, 2, 2);
  plane->SetNormal(-1.0, -1.0, -1.0);
  clipFilter->SetClipFunction(plane);

  vtkNew<vtkGroupDataSetsFilter> groupFilter;
  groupFilter->SetOutputTypeToPartitionedDataSetCollection();
  groupFilter->SetInputConnection(clipFilter->GetOutputPort(0));
  groupFilter->UpdatePiece(myRank, nProcs, 0);

  // print the number of vertices on each partition after clipping
  std::cout << "CLIPPED: rank " << myRank << " has "
            << groupFilter->GetOutput()->GetNumberOfElements(0) << "\n";

  return vtkPartitionedDataSetCollection::SafeDownCast(groupFilter->GetOutput());
}

//------------------------------------------------------------------------------
void RedistributeAndCheck(vtkDataObject* dataset, vtkProcessGroup* subGroup)
{
  const int nProcs = Controller->GetNumberOfProcesses();
  const int myRank = Controller->GetLocalProcessId();

  // redistribute to sub group
  vtkNew<vtkRedistributeDataSetToSubCommFilter> rdsc;
  rdsc->SetSubGroup(subGroup);
  rdsc->SetInputData(dataset);
  rdsc->SetController(Controller);

  double t1, t2, elapsedTime;
  t1 = MPI_Wtime();
  rdsc->Update();
  t2 = MPI_Wtime();
  elapsedTime = t2 - t1;
  LogMessage("elapsed time: " + vtk::to_string(elapsedTime));

  for (int r = 0; r < nProcs; ++r)
  {
    if (myRank == r)
    {
      vtkIdType numPoints = rdsc->GetOutput()->GetNumberOfElements(0);

      int foundIt = subGroup->FindProcessId(r);
      if (foundIt == -1)
      {
        assert(numPoints == 0);
        std::cout << "REPARTITIONED: rank " << r << " not in subgroup has no points" << std::endl;
      }
      else
      {
        assert(numPoints > 0);
        std::cout << "REPARTITIONED: rank " << r << " in subgroup has " << numPoints << " points"
                  << std::endl;
      }
    }
  }
}

} // end of namespace

//------------------------------------------------------------------------------
int TestReducePartitions(int argc, char* argv[])
{
  // Initialize
  Controller = vtkMPIController::New();
  Controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(Controller);

  vtkLogger::SetThreadName("rank: " + vtk::to_string(Controller->GetLocalProcessId()));

  // create a vtkProcessGroup to represent the nodes where data should be aggregated
  int nTargetProcs = 2;
  vtkNew<vtkProcessGroup> subGroup;
  subGroup->Initialize(Controller);
  subGroup->RemoveAllProcessIds();
  for (int i = 0; i < nTargetProcs; ++i)
  {
    subGroup->AddProcessId(i);
  }

  LogMessage(" ---------- Testing redistribution of vtkPartitionedDatasetCollection ---------- ");
  vtkSmartPointer<vtkPartitionedDataSetCollection> pdsc = CreatePartitionedDatasetCollection();
  RedistributeAndCheck(pdsc, subGroup);

  Controller->Barrier();

  LogMessage(" ---------- Testing redistribution of vtkUnstructuredGrid ---------- ");
  vtkSmartPointer<vtkUnstructuredGrid> ug = CreateUnstructuredGrid();
  RedistributeAndCheck(ug, subGroup);

  Controller->Barrier();

  LogMessage(" ---------- Testing redistribution of vtkImageData ---------- ");
  vtkSmartPointer<vtkImageData> imgData = CreateImageData();
  RedistributeAndCheck(imgData, subGroup);

  Controller->Barrier();

  LogMessage(" ---------- Testing redistribution of vtkPartitionedDataSet ---------- ");
  vtkSmartPointer<vtkPartitionedDataSet> pdc = CreatePartitionedDataSet();
  RedistributeAndCheck(pdc, subGroup);

  // cleanup
  Controller->Finalize();
  Controller->Delete();
  return EXIT_SUCCESS;
}
