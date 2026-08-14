// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDataObjectMeshCache.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkHDFReader.h>
#include <vtkNew.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkTestUtilities.h>
#include <vtkTesting.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>

#include <cstdlib>
#include <iostream>

namespace
{
bool TestMtime(vtkHDFReader* reader, vtkTransformFilter* transformFilter)
{
  vtkMTimeType readerMeshTime[2];
  vtkMTimeType transformMeshTime[2];
  readerMeshTime[0] =
    vtkDataObjectMeshCache::GetDataObjectMeshMTime(reader->GetOutputDataObject(0));
  transformMeshTime[0] =
    vtkDataObjectMeshCache::GetDataObjectMeshMTime(transformFilter->GetOutputDataObject(0));

  vtkIdType timeStep = 1;
  reader->SetStep(timeStep);
  transformFilter->Update();

  readerMeshTime[1] =
    vtkDataObjectMeshCache::GetDataObjectMeshMTime(reader->GetOutputDataObject(0));
  transformMeshTime[1] =
    vtkDataObjectMeshCache::GetDataObjectMeshMTime(transformFilter->GetOutputDataObject(0));
  if (readerMeshTime[1] != readerMeshTime[0])
  {
    std::cerr << "Error when reading input file, expecting same Mesh MTime for timesteps 0 and 1\n";
    std::cerr << "Has " << readerMeshTime[0] << " and " << readerMeshTime[1] << "\n";
    return false;
  }

  if (transformMeshTime[0] != transformMeshTime[1])
  {
    std::cerr << "Cache was not used while input have same MTime\n";
    std::cerr << "Output has " << transformMeshTime[0] << " and " << transformMeshTime[1] << "\n";
    return false;
  }

  return true;
}

bool TestArrayUpdate(vtkHDFReader* reader, vtkTransformFilter* transformFilter)
{
  // Warping array is a vector array. As we rotate 90 around Y, its X component become Z.
  vtkPartitionedDataSet* pds = vtkPartitionedDataSet::SafeDownCast(reader->GetOutputDataObject(0));
  auto part = pds->GetPartition(0);
  auto pd = part->GetPointData();
  auto array = pd->GetArray("Warping");

  vtkNew<vtkDoubleArray> baselineXComponent;
  const int X = 0;
  array->GetData(0, array->GetNumberOfTuples() - 1, X, X, baselineXComponent);

  auto transformed = vtkPartitionedDataSet::SafeDownCast(transformFilter->GetOutputDataObject(0));
  auto outPart = transformed->GetPartition(0);
  auto outPd = outPart->GetPointData();
  auto outArray = outPd->GetArray("Warping");
  vtkNew<vtkDoubleArray> outDataZComponent;

  // After rotation, X comp is in Z axis.
  const int Z = 2;
  outArray->GetData(0, outArray->GetNumberOfTuples() - 1, Z, Z, outDataZComponent);

  std::cerr << outArray->GetNumberOfTuples() << "\n";
  std::cerr << array->GetComponent(8, 0) << " - " << outArray->GetComponent(8, 2) << "\n";
  return vtkTestUtilities::CompareAbstractArray(outDataZComponent, baselineXComponent);
}
}

//------------------------------------------------------------------------------
int TestTransformFilterCache(int argc, char* argv[])
{
  vtkNew<vtkTesting> testUtils;
  testUtils->AddArguments(argc, argv);
  std::string dataRoot = testUtils->GetDataRoot();

  vtkNew<vtkHDFReader> reader;
  reader->SetFileName(
    (dataRoot + "/Data/vtkHDF/temporal_partitioned_polydata_cache.vtkhdf").c_str());

  vtkNew<vtkTransformFilter> transformFilter;
  transformFilter->SetInputConnection(reader->GetOutputPort());
  transformFilter->SetTransformAllInputVectors(true);

  vtkNew<vtkTransform> transform;
  // rotate so X becomes Z in Warp data array.
  transform->RotateY(-90);
  transformFilter->SetTransform(transform);
  transformFilter->Update();

  // Generic Time data checks
  if (reader->GetNumberOfSteps() != 10)
  {
    std::cerr << "Number of time steps is not correct: " << reader->GetNumberOfSteps()
              << " != " << 10 << std::endl;
    return EXIT_FAILURE;
  }

  if (!::TestMtime(reader, transformFilter))
  {
    return EXIT_FAILURE;
  }

  if (!::TestArrayUpdate(reader, transformFilter))
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
