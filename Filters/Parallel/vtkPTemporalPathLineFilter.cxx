// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkPTemporalPathLineFilter.h"

#include "vtkAlgorithm.h"
#include "vtkDataSet.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkSmartPointer.h"

#include "vtkDataArrayRange.h"

#include <algorithm>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkPTemporalPathLineFilter);
vtkCxxSetObjectMacro(vtkPTemporalPathLineFilter, Controller, vtkMultiProcessController);

//-----------------------------------------------------------------------------
vtkPTemporalPathLineFilter::vtkPTemporalPathLineFilter()
{
  this->Controller = nullptr;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}

//-----------------------------------------------------------------------------
vtkPTemporalPathLineFilter::~vtkPTemporalPathLineFilter()
{
  this->SetController(nullptr);
}

//-----------------------------------------------------------------------------
void vtkPTemporalPathLineFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Controller: " << this->Controller << "\n";
}

//-----------------------------------------------------------------------------
int vtkPTemporalPathLineFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  if (port == 0)
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPartitionedDataSet");
  }
  else if (port == 1)
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  }
  return 1;
}

//-----------------------------------------------------------------------------
int vtkPTemporalPathLineFilter::FillOutputPortInformation(int port, vtkInformation* info)
{
  if (port == 0 || port == 1)
  {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  }
  return 1;
}

//-----------------------------------------------------------------------------
vtkDataSet* vtkPTemporalPathLineFilter::GetFirstNonEmptyPartition(vtkPartitionedDataSet* pds)
{
  for (unsigned int i = 0; i < pds->GetNumberOfPartitions(); ++i)
  {
    auto* ds = vtkDataSet::SafeDownCast(pds->GetPartition(i));
    if (ds && ds->GetNumberOfPoints() > 0)
    {
      return ds;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkPTemporalPathLineFilter::AllGatherSelection(vtkDataSet* selection)
{
  if (!selection)
  {
    return nullptr;
  }

  if (!this->Controller || this->Controller->GetNumberOfProcesses() <= 1)
  {
    // Single process: just return the original selection as-is.
    return selection;
  }

  // Extract local selection IDs.
  vtkPointData* selPD = selection->GetPointData();
  vtkDataArray* selIds = this->IdChannelArray ? selPD->GetArray(this->IdChannelArray) : nullptr;
  selIds = selIds ? selIds : selPD->GetGlobalIds();

  if (!selIds)
  {
    // No ID array found — cannot allgather. Return original.
    return selection;
  }

  // Gather local IDs into a vtkIdTypeArray.
  vtkNew<vtkIdTypeArray> localIds;
  localIds->SetNumberOfTuples(selIds->GetNumberOfTuples());
  for (vtkIdType i = 0; i < selIds->GetNumberOfTuples(); ++i)
  {
    localIds->SetValue(i, static_cast<vtkIdType>(selIds->GetTuple1(i)));
  }

  // AllGatherV to collect all IDs across ranks.
  vtkNew<vtkIdTypeArray> gatheredIds;
  this->Controller->AllGatherV(localIds, gatheredIds);

  // Sort and remove duplicates — multiple ranks may have the same selected IDs.
  auto range = vtk::DataArrayValueRange<1>(gatheredIds);
  std::vector<vtkIdType> sorted(range.begin(), range.end());
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

  vtkNew<vtkIdTypeArray> uniqueIds;
  uniqueIds->SetNumberOfTuples(static_cast<vtkIdType>(sorted.size()));
  for (vtkIdType i = 0; i < static_cast<vtkIdType>(sorted.size()); ++i)
  {
    uniqueIds->SetValue(i, sorted[i]);
  }

  // Build a local vtkPolyData with the unique IDs as point data
  // so AccumulateTrails can find them. Allocate matching dummy
  // points so GetNumberOfPoints() stays consistent with the
  // point-data array length.
  vtkNew<vtkPolyData> gatheredSelection;
  vtkNew<vtkPoints> dummyPoints;
  dummyPoints->SetNumberOfPoints(uniqueIds->GetNumberOfTuples());
  gatheredSelection->SetPoints(dummyPoints);

  if (this->IdChannelArray)
  {
    uniqueIds->SetName(this->IdChannelArray);
    gatheredSelection->GetPointData()->AddArray(uniqueIds);
  }
  else
  {
    gatheredSelection->GetPointData()->SetGlobalIds(uniqueIds);
  }

  return gatheredSelection;
}

//-----------------------------------------------------------------------------
int vtkPTemporalPathLineFilter::Initialize(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkDataObject* inputDO = inInfo->Get(vtkDataObject::DATA_OBJECT());

  auto* pds = vtkPartitionedDataSet::SafeDownCast(inputDO);
  if (!pds)
  {
    // Plain vtkDataSet input — delegate to superclass.
    return this->Superclass::Initialize(request, inputVector, outputVector);
  }

  // Partitioned dataset input.
  this->MaskPoints = std::max(this->MaskPoints, 1);

  vtkInformation* outInfo0 = outputVector->GetInformationObject(0);
  vtkPolyData* pathLines = vtkPolyData::SafeDownCast(outInfo0->Get(vtkDataObject::DATA_OBJECT()));

  this->Flush();

  vtkDataSet* firstPartition = this->GetFirstNonEmptyPartition(pds);
  if (firstPartition)
  {
    this->InitializeExecute(firstPartition, pathLines);
  }

  return 1;
}

//-----------------------------------------------------------------------------
int vtkPTemporalPathLineFilter::Execute(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* vtkNotUsed(outputVector))
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* selInfo = inputVector[1]->GetInformationObject(0);
  vtkDataObject* inputDO = inInfo->Get(vtkDataObject::DATA_OBJECT());

  vtkDataSet* selection =
    selInfo ? vtkDataSet::SafeDownCast(selInfo->Get(vtkDataObject::DATA_OBJECT())) : nullptr;

  // Allgather the selection so all ranks know which particles to track.
  vtkSmartPointer<vtkDataSet> gatheredSelection = this->AllGatherSelection(selection);

  auto* pds = vtkPartitionedDataSet::SafeDownCast(inputDO);
  if (!pds)
  {
    // Plain vtkDataSet input.
    auto* input = vtkDataSet::SafeDownCast(inputDO);
    this->AccumulateTrails(input, gatheredSelection);
    return 1;
  }

  // Partitioned dataset input — iterate over all partitions.
  // Clear marks once before processing all partitions, then remove dead once after.
  // This prevents partitions from clearing each other's alive flags.
  this->ClearTrailMarks();
  for (unsigned int i = 0; i < pds->GetNumberOfPartitions(); ++i)
  {
    auto* partition = vtkDataSet::SafeDownCast(pds->GetPartition(i));
    if (partition && partition->GetNumberOfPoints() > 0)
    {
      this->ProcessTrails(partition, gatheredSelection);
    }
  }
  this->RemoveDeadTrails();

  return 1;
}

//-----------------------------------------------------------------------------
int vtkPTemporalPathLineFilter::Finalize(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkDataObject* inputDO = inInfo->Get(vtkDataObject::DATA_OBJECT());

  auto* pds = vtkPartitionedDataSet::SafeDownCast(inputDO);
  if (!pds)
  {
    // Plain vtkDataSet input — delegate to superclass.
    return this->Superclass::Finalize(request, inputVector, outputVector);
  }

  // Partitioned dataset input.
  vtkInformation* outInfo0 = outputVector->GetInformationObject(0);
  vtkInformation* outInfo1 = outputVector->GetInformationObject(1);

  vtkPolyData* pathLines = vtkPolyData::SafeDownCast(outInfo0->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData* particles = vtkPolyData::SafeDownCast(outInfo1->Get(vtkDataObject::DATA_OBJECT()));

  vtkDataSet* firstPartition = this->GetFirstNonEmptyPartition(pds);
  if (firstPartition)
  {
    this->PostExecute(firstPartition, pathLines, particles);
  }

  return 1;
}

VTK_ABI_NAMESPACE_END
