// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkYieldCriteria.h"

#include "vtkArrayDispatch.h"
#include "vtkCellData.h"
#include "vtkCommand.h"
#include "vtkDataArray.h"
#include "vtkDataArrayRange.h"
#include "vtkDataArraySelection.h"
#include "vtkDataSet.h"
#include "vtkDataSetAttributes.h"
#include "vtkDoubleArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkSMPTools.h"
#include "vtkTensorPrincipalInvariants.h"

#include <cmath>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkYieldCriteria);

namespace
{
const std::map<vtkYieldCriteria::Criterion, std::string> Names = {
  { vtkYieldCriteria::Criterion::PrincipalStress, "Principal Stress" },
  { vtkYieldCriteria::Criterion::Tresca, "Tresca Criterion" },
  { vtkYieldCriteria::Criterion::VonMises, "Von Mises Criterion" }
};

/**
 * Worker used to compute yield criteria in parallel with vtkArrayDispatch and vtkSMPTools.
 */
struct YieldWorker
{
  bool ComputeTresca = false;
  bool ComputeVonMises = false;

  template <typename Array1T>
  void operator()(Array1T* a1, vtkDoubleArray* sigma1, vtkDoubleArray* sigma2,
    vtkDoubleArray* sigma3, vtkDoubleArray* tresca, vtkDoubleArray* vonMises)
  {
    auto inTuples = vtk::DataArrayTupleRange(a1);

    auto sigma1Range = vtk::DataArrayValueRange<1>(sigma1);
    auto sigma2Range = vtk::DataArrayValueRange<1>(sigma2);
    auto sigma3Range = vtk::DataArrayValueRange<1>(sigma3);

    auto outTresca = vtk::DataArrayValueRange<1>(tresca);
    auto outVonMises = vtk::DataArrayValueRange<1>(vonMises);

    const vtkIdType numTuples = sigma1Range.size();

    vtkSMPTools::For(0, numTuples,
      [&](vtkIdType begin, vtkIdType end)
      {
        for (vtkIdType i = begin; i < end; ++i)
        {
          if (std::isnan(inTuples[i][0]))
          {
            if (ComputeTresca)
            {
              outTresca[i] = std::nan("");
            }

            if (ComputeVonMises)
            {
              outVonMises[i] = std::nan("");
            }
            continue;
          }

          const double value1 = sigma1Range[i];
          const double value2 = sigma2Range[i];
          const double value3 = sigma3Range[i];

          if (ComputeTresca)
          {
            outTresca[i] = std::abs(value3 - value1);
          }

          if (ComputeVonMises)
          {
            outVonMises[i] =
              std::sqrt((value1 - value2) * (value1 - value2) +
                (value2 - value3) * (value2 - value3) + (value1 - value3) * (value1 - value3)) /
              std::sqrt(2.0);
          }
        }
      });
  }
};
}

//------------------------------------------------------------------------------
vtkYieldCriteria::vtkYieldCriteria()
{
  this->PointDataArraySelection->AddObserver(
    vtkCommand::ModifiedEvent, this, &vtkYieldCriteria::Modified);
  this->CellDataArraySelection->AddObserver(
    vtkCommand::ModifiedEvent, this, &vtkYieldCriteria::Modified);
  this->CriteriaSelection->AddObserver(
    vtkCommand::ModifiedEvent, this, &vtkYieldCriteria::Modified);
}

//------------------------------------------------------------------------------
int vtkYieldCriteria::RequestInformation(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // Call RequestInformation in the principal invariants filter to fill the
  // selection list of available point and cell arrays
  // (arrays with 3 components named "XX", "YY", "XY" or with 6 components)
  if (this->InvariantsFilter->ProcessRequest(request, inputVector, outputVector) == 0)
  {
    return 0;
  }

  vtkDataArraySelection* pointArraySelection = this->InvariantsFilter->GetPointDataArraySelection();
  vtkDataArraySelection* cellArraySelection = this->InvariantsFilter->GetCellDataArraySelection();

  for (vtkIdType idx = 0; idx < pointArraySelection->GetNumberOfArrays(); idx++)
  {
    this->PointDataArraySelection->AddArray(pointArraySelection->GetArrayName(idx));
  }

  for (vtkIdType idx = 0; idx < cellArraySelection->GetNumberOfArrays(); idx++)
  {
    this->CellDataArraySelection->AddArray(cellArraySelection->GetArrayName(idx));
  }

  // Fill selection list of yield criteria
  this->CriteriaSelection->AddArray(Names.at(Criterion::PrincipalStress).c_str());
  this->CriteriaSelection->AddArray(Names.at(Criterion::Tresca).c_str());
  this->CriteriaSelection->AddArray(Names.at(Criterion::VonMises).c_str());

  return 1;
}

//------------------------------------------------------------------------------
int vtkYieldCriteria::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkDataSet* input = vtkDataSet::GetData(inputVector[0]);
  vtkDataSet* output = vtkDataSet::GetData(outputVector);

  if (!input || !output)
  {
    vtkErrorMacro("Could not retrieve input or output.");
    return 0;
  }

  if (this->CriteriaSelection->GetNumberOfArraysEnabled() == 0)
  {
    output->ShallowCopy(input);
    return 1;
  }

  // Compute principal values and vectors
  this->InvariantsFilter->SetScaleVectors(this->ScaleVectors);
  this->InvariantsFilter->GetPointDataArraySelection()->CopySelections(
    this->PointDataArraySelection);
  this->InvariantsFilter->GetCellDataArraySelection()->CopySelections(this->CellDataArraySelection);

  if (this->InvariantsFilter->ProcessRequest(request, inputVector, outputVector) == 0)
  {
    return 0;
  }

  // Retrieve output that contains principal invariants
  output = vtkDataSet::GetData(outputVector);

  if (!output)
  {
    vtkErrorMacro("Could not retrieve output.");
    return 0;
  }

  vtkPointData* pointData = input->GetPointData();
  vtkCellData* cellData = input->GetCellData();
  vtkIdType nbPoints = input->GetNumberOfPoints();
  vtkIdType nbCells = input->GetNumberOfCells();

  // Compute yield criteria for selected point arrays
  for (vtkIdType idx = 0; idx < this->PointDataArraySelection->GetNumberOfArrays(); idx++)
  {
    if (this->PointDataArraySelection->GetArraySetting(idx) == 0)
    {
      continue;
    }

    // Retrieve array from name
    std::string arrayName = this->PointDataArraySelection->GetArrayName(idx);
    vtkDataArray* array = pointData->GetArray(arrayName.c_str());

    if (!array)
    {
      vtkWarningMacro("Could not retrieve point array '" << arrayName << "', skipping.");
      continue;
    }

    // Compute derived yield criteria data arrays
    if (!this->ComputeYieldCriteria(output, array, arrayName, nbPoints, true))
    {
      vtkWarningMacro(
        "Could not compute yield criteria for point array '" << arrayName << "', skipping.");
      continue;
    }
  }

  // Compute yield criteria for selected cell arrays
  for (vtkIdType idx = 0; idx < this->CellDataArraySelection->GetNumberOfArrays(); idx++)
  {
    if (this->CellDataArraySelection->GetArraySetting(idx) == 0)
    {
      continue;
    }

    // Retrieve array from name
    std::string arrayName = this->CellDataArraySelection->GetArrayName(idx);
    vtkDataArray* array = cellData->GetArray(arrayName.c_str());

    if (!array)
    {
      vtkWarningMacro("Could not retrieve cell array '" << arrayName << "', skipping.");
      continue;
    }

    // Compute derived yield criteria data arrays
    if (!this->ComputeYieldCriteria(output, array, arrayName, nbCells, false))
    {
      vtkWarningMacro(
        "Could not compute yield criteria for cell array '" << arrayName << "', skipping.");
      continue;
    }
  }

  return 1;
}

//------------------------------------------------------------------------------
bool vtkYieldCriteria::ComputeYieldCriteria(vtkDataSet* output, vtkDataArray* array,
  const std::string& arrayName, vtkIdType nbTuples, bool isPointData) const
{
  // Check number of components for input array
  vtkIdType nbComp = array->GetNumberOfComponents();

  if (nbComp != 3 && nbComp != 6)
  {
    vtkWarningMacro("Array '" << arrayName << "' does not have 3 or 6 components, skipping.");
    return false;
  }

  bool keepStress =
    this->CriteriaSelection->ArrayIsEnabled(Names.at(Criterion::PrincipalStress).c_str());
  bool computeTresca = this->CriteriaSelection->ArrayIsEnabled(Names.at(Criterion::Tresca).c_str());
  bool computeVonMises =
    this->CriteriaSelection->ArrayIsEnabled(Names.at(Criterion::VonMises).c_str());

  // Retrieve principal values
  vtkDataSetAttributes* attributes = isPointData
    ? vtkDataSetAttributes::SafeDownCast(output->GetPointData())
    : vtkDataSetAttributes::SafeDownCast(output->GetCellData());

  vtkDoubleArray* sigma1 = vtkArrayDownCast<vtkDoubleArray>(attributes->GetArray(
    vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 1).c_str()));
  vtkDoubleArray* sigma2 = vtkArrayDownCast<vtkDoubleArray>(attributes->GetArray(
    vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 2).c_str()));
  vtkDoubleArray* sigma3 = vtkArrayDownCast<vtkDoubleArray>(attributes->GetArray(
    vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 3).c_str()));

  if (!sigma1 || !sigma2 || !sigma3)
  {
    vtkWarningMacro(
      "Could not retrieve principal values for array '" << arrayName << "', skipping.");
    return false;
  }

  // Create derived data arrays
  vtkNew<vtkDoubleArray> tresca;
  vtkNew<vtkDoubleArray> vonMises;

  tresca->SetName((arrayName + " - Tresca Criterion").c_str());
  vonMises->SetName((arrayName + " - Von Mises Criterion").c_str());

  if (computeTresca)
  {
    tresca->SetNumberOfTuples(nbTuples);
  }
  if (computeVonMises)
  {
    vonMises->SetNumberOfTuples(nbTuples);
  }

  YieldWorker worker;
  worker.ComputeTresca = computeTresca;
  worker.ComputeVonMises = computeVonMises;

  using MyDispatch = vtkArrayDispatch::DispatchByValueType<vtkArrayDispatch::Reals>;
  if (!MyDispatch::Execute(
        array, worker, sigma1, sigma2, sigma3, tresca.GetPointer(), vonMises.GetPointer()))
  {
    worker(array, sigma1, sigma2, sigma3, tresca.GetPointer(), vonMises.GetPointer());
  }

  // Add arrays to output
  if (!keepStress)
  {
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaVectorArrayName(arrayName, 1).c_str());
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaVectorArrayName(arrayName, 2).c_str());
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaVectorArrayName(arrayName, 3).c_str());
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 1).c_str());
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 2).c_str());
    attributes->RemoveArray(
      vtkTensorPrincipalInvariants::GetSigmaValueArrayName(arrayName, 3).c_str());
  }

  if (computeTresca)
  {
    attributes->AddArray(tresca);
  }

  if (computeVonMises)
  {
    attributes->AddArray(vonMises);
  }

  return true;
}

//------------------------------------------------------------------------------
void vtkYieldCriteria::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ScaleVectors: " << this->ScaleVectors << endl;
}
VTK_ABI_NAMESPACE_END
