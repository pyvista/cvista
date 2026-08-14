// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "HDFTestUtilities.h"

#include "vtkAlgorithm.h"
#include "vtkCellArray.h"
#include "vtkDataObject.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkFieldData.h"
#include "vtkGroupDataSetsFilter.h"
#include "vtkHDFReader.h"
#include "vtkHDFWriter.h"
#include "vtkHyperTreeGrid.h"
#include "vtkHyperTreeGridAlgorithm.h"
#include "vtkHyperTreeGridSource.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkLogger.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPartitionedDataSetCollection.h"
#include "vtkRandomHyperTreeGridSource.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTestUtilities.h"

#include <iostream>
#include <string>

namespace HDFTestUtilities
{
vtkStandardNewMacro(vtkHTGChangingDescriptorSource);
}

//----------------------------------------------------------------------------
bool TestHDFWriterHTGTemporalBase(const std::string& tempDir, bool extTime)
{
  std::string filePath =
    tempDir + "/HDFWriterHTGTemporal" + (extTime ? "ExtTime" : "NoExtTime") + ".vtkhdf";

  vtkNew<HDFTestUtilities::vtkHTGChangingDescriptorSource> source;
  source->SetDescriptors({ ".RRR|..R..... .R...... ........ | ........ ........",
    "RR.R|..R..... .R...... .......R | ........ .R...... ........ | ........" });
  source->SetMasks({ "0111|00111111 01011111 11111111 | 00111111 01011111",
    "1111|00100000 01000000 00000001 | 11111111 11111111 11111111 | 10111111" });
  source->SetDimensions({ 3, 3, 2 });
  vtkNew<vtkHDFWriter> writer;
  writer->SetInputConnection(source->GetOutputPort());
  writer->SetFileName(filePath.c_str());
  writer->SetWriteAllTimeSteps(true);
  writer->SetUseExternalTimeSteps(extTime);
  writer->Write();

  vtkNew<vtkHDFReader> reader;
  reader->SetFileName(filePath.c_str());
  reader->UpdateInformation();

  constexpr int nbSteps = 2;
  if (reader->GetNumberOfSteps() != nbSteps)
  {
    vtkLog(ERROR, "Unexpected number of steps: " << reader->GetNumberOfSteps());
    return false;
  }

  for (int i = 0; i < nbSteps; i++)
  {
    reader->SetStep(i);
    reader->Update();
    vtkHyperTreeGrid* htgRead = vtkHyperTreeGrid::SafeDownCast(reader->GetOutputDataObject(0));

    source->GetOutputInformation(0)->Set(
      vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), static_cast<double>(i));
    source->Modified();
    source->Update();
    vtkHyperTreeGrid* htgSource = vtkHyperTreeGrid::SafeDownCast(source->GetHyperTreeGridOutput());

    // Remove added "Time" field data so the comparison is correct
    vtkNew<vtkFieldData> fd;
    htgRead->SetFieldData(fd);

    if (!vtkTestUtilities::CompareDataObjects(htgRead, htgSource))
    {
      std::cerr << "HTG does not match: " << filePath << " for time step " << i << std::endl;
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool TestHDFWriterHTGTDistributedTemporalPDC(const std::string& tempDir, bool extTime)
{
  std::string filePath =
    tempDir + "/HDFWriterHTGTemporalPDC" + (extTime ? "ExtTime" : "NoExtTime") + ".vtkhdf";

  // Hyper-Tree Art: HyperTetrisGrid, 2026
  vtkNew<HDFTestUtilities::vtkHTGChangingDescriptorSource> source1;
  source1->SetDescriptors(
    { "R|.R.......|.........", "R|.R.......|.........", "R|.R.......|.........", "R|........." });
  source1->SetMasks(
    { "1|111101101|110110100", "1|111101101|110110100", "1|111101101|110110100", "1|111101101" });
  source1->SetDimensions({ 2, 2, 1 });
  source1->SetBranchFactor(3);

  vtkNew<HDFTestUtilities::vtkHTGChangingDescriptorSource> source2;
  source2->SetDescriptors(
    { "R|.......R.|.........", "R|....R....|.........", "R|.R.......|.........", "." });
  source2->SetMasks(
    { "1|000000010|001001011", "1|000010000|001001011", "1|010000000|001001011", "0" });
  source2->SetDimensions({ 2, 2, 1 });
  source2->SetBranchFactor(3);

  vtkNew<vtkGroupDataSetsFilter> pdsGroup;
  pdsGroup->SetOutputTypeToPartitionedDataSet();
  pdsGroup->AddInputConnection(source1->GetOutputPort());
  pdsGroup->AddInputConnection(source2->GetOutputPort());

  vtkNew<vtkRandomHyperTreeGridSource> htgSource;
  htgSource->SetOutputBounds(-6, -3, -4, -1, -5, -1);
  htgSource->SetMaxDepth(4);

  vtkNew<vtkGroupDataSetsFilter> pdcGroup;
  pdcGroup->SetOutputTypeToPartitionedDataSetCollection();
  pdcGroup->AddInputConnection(pdsGroup->GetOutputPort());
  pdcGroup->AddInputConnection(htgSource->GetOutputPort());

  vtkNew<vtkHDFWriter> writer;
  writer->SetInputConnection(pdcGroup->GetOutputPort());
  writer->SetFileName(filePath.c_str());
  writer->SetWriteAllTimeSteps(true);
  writer->Write();

  vtkNew<vtkHDFReader> reader;
  reader->SetFileName(filePath.c_str());
  reader->UpdateInformation();

  constexpr int nbSteps = 4;
  if (reader->GetNumberOfSteps() != nbSteps)
  {
    vtkLog(ERROR, "Unexpected number of steps: " << reader->GetNumberOfSteps());
    return false;
  }

  for (int i = 0; i < nbSteps; i++)
  {
    reader->SetStep(i);
    reader->Update();
    vtkPartitionedDataSetCollection* pdcRead =
      vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));

    pdcGroup->GetOutputInformation(0)->Set(
      vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), static_cast<double>(i));
    pdcGroup->Modified();
    pdcGroup->Update();
    vtkPartitionedDataSetCollection* pdcSource =
      vtkPartitionedDataSetCollection::SafeDownCast(pdcGroup->GetOutputDataObject(0));

    // Remove added "Time" field data so the comparison is correct
    vtkNew<vtkFieldData> fd;
    pdcRead->GetPartitionedDataSet(0)->GetPartitionAsDataObject(0)->SetFieldData(fd);
    pdcRead->GetPartitionedDataSet(0)->GetPartitionAsDataObject(1)->SetFieldData(fd);
    pdcRead->GetPartitionedDataSet(1)->GetPartitionAsDataObject(0)->SetFieldData(fd);

    pdcSource->SetDataAssembly(pdcRead->GetDataAssembly());

    if (!vtkTestUtilities::CompareDataObjects(pdcRead, pdcSource))
    {
      std::cerr << "Partitioned composite data does not match: " << filePath << " for time step "
                << i << std::endl;
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
int TestHDFWriterHTGTemporal(int argc, char* argv[])
{
  char* tempDirCStr =
    vtkTestUtilities::GetArgOrEnvOrDefault("-T", argc, argv, "VTK_TEMP_DIR", "Testing/Temporary");
  std::string tempDir{ tempDirCStr };
  delete[] tempDirCStr;

  bool testPasses = true;
  testPasses &= TestHDFWriterHTGTemporalBase(tempDir, false);
  testPasses &= TestHDFWriterHTGTemporalBase(tempDir, true);
  testPasses &= TestHDFWriterHTGTDistributedTemporalPDC(tempDir, false);
  testPasses &= TestHDFWriterHTGTDistributedTemporalPDC(tempDir, true);
  return testPasses ? EXIT_SUCCESS : EXIT_FAILURE;
}
