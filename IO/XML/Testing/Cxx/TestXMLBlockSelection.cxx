// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCompositeDataSet.h"
#include "vtkConeSource.h"
#include "vtkCylinderSource.h"
#include "vtkInformation.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiPieceDataSet.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPartitionedDataSetCollection.h"
#include "vtkSphereSource.h"
#include "vtkTesting.h"
#include "vtkXMLMultiBlockDataReader.h"
#include "vtkXMLMultiBlockDataWriter.h"
#include "vtkXMLPartitionedDataSetCollectionReader.h"
#include "vtkXMLPartitionedDataSetCollectionWriter.h"

#include <iostream>

// Test for vtkXMLPartitionedDataSetCollectionReader::(Set/Add/Clear)Selector
bool TestPartitionSelection(const std::string& tempDir)
{
  // Create a PDC with 3 partitioned datasets
  vtkNew<vtkPartitionedDataSetCollection> pdc;

  vtkNew<vtkSphereSource> sphere;
  sphere->Update();
  vtkNew<vtkConeSource> cone;
  cone->Update();
  vtkNew<vtkCylinderSource> cylinder;
  cylinder->Update();

  pdc->SetPartition(0, 0, sphere->GetOutput());
  pdc->SetPartition(1, 0, cone->GetOutput());
  pdc->SetPartition(2, 0, cylinder->GetOutput());
  pdc->GetMetaData(0u)->Set(vtkCompositeDataSet::NAME(), "Sphere");
  pdc->GetMetaData(1)->Set(vtkCompositeDataSet::NAME(), "Cone");
  pdc->GetMetaData(2)->Set(vtkCompositeDataSet::NAME(), "Cylinder");

  // Write it
  std::string filePath = tempDir + "/TestPartitionSelection.vtpc";
  vtkNew<vtkXMLPartitionedDataSetCollectionWriter> writer;
  writer->SetInputData(pdc);
  writer->SetFileName(filePath.c_str());
  writer->Write();

  // Read it back and check that GetPartitionSelection is populated
  vtkNew<vtkXMLPartitionedDataSetCollectionReader> reader;
  reader->SetFileName(filePath.c_str());

  // Must call UpdateInformation to populate the assembly
  reader->UpdateInformation();

  // All blocks should be enabled by default
  if (reader->GetNumberOfSelectors() != 1 || !reader->GetSelector(0) ||
    strcmp(reader->GetSelector(0), "/") != 0)
  {
    std::cerr << "Expected 1 selector with value '/', got " << reader->GetNumberOfSelectors()
              << " and '" << (reader->GetSelector(0) ? reader->GetSelector(0) : "nullptr") << "'"
              << std::endl;
    return false;
  }
  // check that there are 0 cell arrays and 2 point arrays
  if (reader->GetNumberOfCellArrays() != 0 || reader->GetNumberOfPointArrays() != 2)
  {
    std::cerr << "Expected 0 cell arrays and 2 point arrays, got "
              << reader->GetNumberOfCellArrays() << " and " << reader->GetNumberOfPointArrays()
              << std::endl;
    return false;
  }
  // Enable block 0 (Sphere), block 2 (Cylinder) and read
  reader->ClearSelectors();
  reader->AddSelector("/Root/Sphere");
  reader->AddSelector("/Root/Cylinder");
  reader->Update();

  vtkPartitionedDataSetCollection* output =
    vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutput());
  if (!output)
  {
    std::cerr << "Output is not a vtkPartitionedDataSetCollection" << std::endl;
    return false;
  }

  // check that is has 3 partitioned dataset
  if (output->GetNumberOfPartitionedDataSets() != 3)
  {
    std::cerr << "Expected 3 PartitionedDataSets, got " << output->GetNumberOfPartitionedDataSets()
              << std::endl;
    return false;
  }

  // Partition 0 (Sphere) should have data
  if (!output->GetPartitionedDataSet(0) ||
    output->GetPartitionedDataSet(0)->GetPartition(0) == nullptr)
  {
    std::cerr << "Partition 0 (Sphere) should have been read" << std::endl;
    return false;
  }

  // Partition 1 (Cone) should be empty/null since we disabled it
  if (output->GetPartitionedDataSet(1) &&
    output->GetPartitionedDataSet(1)->GetPartition(0) != nullptr)
  {
    std::cerr << "Partition 1 (Cone) should not have been read" << std::endl;
    return false;
  }

  // Partition 2 (Cylinder) should have data
  if (!output->GetPartitionedDataSet(2) ||
    output->GetPartitionedDataSet(2)->GetPartition(0) == nullptr)
  {
    std::cerr << "Partition 2 (Cylinder) should have been read" << std::endl;
    return false;
  }

  // Re-enable all and verify all 3 are read
  reader->SetSelector("/");
  reader->Update();
  output = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutput());
  for (unsigned int i = 0; i < output->GetNumberOfPartitionedDataSets(); ++i)
  {
    if (!output->GetPartitionedDataSet(i) ||
      output->GetPartitionedDataSet(i)->GetPartition(0) == nullptr)
    {
      std::cerr << "Partition " << i << " should have been read after re-enabling all" << std::endl;
      return false;
    }
  }

  return true;
}

enum TestBlockType
{
  DataSet,
  MultiPiece,
  MultiBlock
};

vtkSmartPointer<vtkDataObject> CreateBlock(vtkDataSet* dataset, TestBlockType blockType)
{
  switch (blockType)
  {
    case DataSet:
      return dataset;
    case MultiPiece:
    {
      vtkNew<vtkMultiPieceDataSet> multiPiece;
      multiPiece->SetNumberOfPieces(1);
      multiPiece->SetPiece(0, dataset);
      return multiPiece;
    }
    case MultiBlock:
    {
      vtkNew<vtkMultiBlockDataSet> multiBlock;
      multiBlock->SetNumberOfBlocks(1);
      multiBlock->SetBlock(0, dataset);
      return multiBlock;
    }
    default:
      return nullptr;
  }
}

// Test for vtkXMLMultiBlockDataReader::(Set/Add/Clear)Selector
bool TestBlockSelection(const std::string& tempDir, TestBlockType blockType)
{
  // Create a MultiBlock with 3 blocks
  vtkNew<vtkMultiBlockDataSet> mb;
  mb->SetNumberOfBlocks(3);

  vtkNew<vtkSphereSource> sphere;
  sphere->Update();
  vtkNew<vtkConeSource> cone;
  cone->Update();
  vtkNew<vtkCylinderSource> cylinder;
  cylinder->Update();

  mb->SetBlock(0, CreateBlock(sphere->GetOutput(), blockType));
  mb->SetBlock(1, CreateBlock(cone->GetOutput(), blockType));
  mb->SetBlock(2, CreateBlock(cylinder->GetOutput(), blockType));
  mb->GetMetaData(0u)->Set(vtkCompositeDataSet::NAME(), "Sphere");
  mb->GetMetaData(1u)->Set(vtkCompositeDataSet::NAME(), "Cone");
  mb->GetMetaData(2u)->Set(vtkCompositeDataSet::NAME(), "Cylinder");

  // Write it
  std::string filePath = tempDir + "/TestBlockSelection.vtm";
  vtkNew<vtkXMLMultiBlockDataWriter> writer;
  writer->SetInputData(mb);
  writer->SetFileName(filePath.c_str());
  writer->Write();

  // Read it back and check that GetBlockSelection is populated
  vtkNew<vtkXMLMultiBlockDataReader> reader;
  reader->SetFileName(filePath.c_str());

  // Must call UpdateInformation to populate the assembly
  reader->UpdateInformation();

  if (reader->GetNumberOfSelectors() != 1 || !reader->GetSelector(0) ||
    strcmp(reader->GetSelector(0), "/") != 0)
  {
    std::cerr << "Expected 1 selector with value '/', got " << reader->GetNumberOfSelectors()
              << " and '" << (reader->GetSelector(0) ? reader->GetSelector(0) : "nullptr") << "'"
              << std::endl;
    return false;
  }

  // check that there are 0 cell arrays and 2 point arrays
  if (reader->GetNumberOfCellArrays() != 0 || reader->GetNumberOfPointArrays() != 2)
  {
    std::cerr << "Expected 0 cell arrays and 2 point arrays, got "
              << reader->GetNumberOfCellArrays() << " and " << reader->GetNumberOfPointArrays()
              << std::endl;
    return false;
  }
  // Enable block 0 (Sphere), block 2 (Cylinder) and read
  reader->ClearSelectors();
  reader->AddSelector("/Root/Sphere");
  reader->AddSelector("/Root/Cylinder");
  reader->Update();

  vtkMultiBlockDataSet* output = vtkMultiBlockDataSet::SafeDownCast(reader->GetOutput());
  if (!output)
  {
    std::cerr << "Output is not a vtkMultiBlockDataSet" << std::endl;
    return false;
  }

  // check that is has 3 blocks
  if (output->GetNumberOfBlocks() != 3)
  {
    std::cerr << "Expected 3 Blocks, got " << output->GetNumberOfBlocks() << std::endl;
    return false;
  }

  // Block 0 (Sphere) should have data
  if (output->GetBlock(0) == nullptr ||
    output->GetBlock(0)->GetNumberOfElements(vtkDataObject::CELL) == 0)
  {
    std::cerr << "Block 0 (Sphere) should have been read" << std::endl;
    return false;
  }

  // Block 1 (Cone) should be null since we disabled it
  if (output->GetBlock(1) != nullptr &&
    output->GetBlock(1)->GetNumberOfElements(vtkDataObject::CELL) != 0)
  {
    std::cerr << "Block 1 (Cone) should not have been read" << std::endl;
    return false;
  }

  // Block 2 (Cylinder) should have data
  if (output->GetBlock(2) == nullptr ||
    output->GetBlock(2)->GetNumberOfElements(vtkDataObject::CELL) == 0)
  {
    std::cerr << "Block 2 (Cylinder) should have been read" << std::endl;
    return false;
  }

  // Re-enable all and verify all 3 are read
  reader->SetSelector("/");
  reader->Update();
  output = vtkMultiBlockDataSet::SafeDownCast(reader->GetOutput());
  for (unsigned int i = 0; i < output->GetNumberOfBlocks(); ++i)
  {
    if (output->GetBlock(i) == nullptr ||
      output->GetBlock(i)->GetNumberOfElements(vtkDataObject::CELL) == 0)
    {
      std::cerr << "Block " << i << " should have been read after re-enabling all" << std::endl;
      return false;
    }
  }

  return true;
}

int TestXMLBlockSelection(int argc, char* argv[])
{
  vtkNew<vtkTesting> testing;
  testing->AddArguments(argc, argv);

  bool result = TestPartitionSelection(testing->GetTempDirectory());
  for (unsigned int i = 0; i < 3; ++i)
  {
    result &= TestBlockSelection(testing->GetTempDirectory(), static_cast<TestBlockType>(i));
  }
  return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
