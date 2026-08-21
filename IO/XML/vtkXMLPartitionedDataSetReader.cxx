// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkXMLPartitionedDataSetReader.h"

#include "vtkCompositeDataPipeline.h"
#include "vtkCompositeDataSet.h"
#include "vtkDataSet.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSet.h"
#include "vtkSmartPointer.h"
#include "vtkXMLDataElement.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkXMLPartitionedDataSetReader);

//------------------------------------------------------------------------------
vtkXMLPartitionedDataSetReader::vtkXMLPartitionedDataSetReader() = default;

//------------------------------------------------------------------------------
vtkXMLPartitionedDataSetReader::~vtkXMLPartitionedDataSetReader() = default;

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
int vtkXMLPartitionedDataSetReader::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPartitionedDataSet");
  return 1;
}

//------------------------------------------------------------------------------
const char* vtkXMLPartitionedDataSetReader::GetDataSetName()
{
  return "vtkPartitionedDataSet";
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetReader::CreateMetaData(vtkXMLDataElement* ePrimary)
{
  auto pds = vtkSmartPointer<vtkPartitionedDataSet>::New();
  const unsigned int numberOfPartitions =
    vtkXMLCompositeDataReader::CountNestedElements(ePrimary, "DataSet");
  pds->SetNumberOfPartitions(numberOfPartitions);
  this->Metadata = pds;
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetReader::SyncCompositeDataArraySelections(
  vtkCompositeDataSet* vtkNotUsed(metadata), vtkXMLDataElement* element,
  const std::string& filePath)
{
  for (int cc = 0; cc < element->GetNumberOfNestedElements(); ++cc)
  {
    vtkXMLDataElement* childXML = element->GetNestedElement(cc);
    if (!childXML || !childXML->GetName())
    {
      continue;
    }
    const char* tagName = childXML->GetName();

    if (strcmp(tagName, "DataSet") == 0)
    {
      int index = 0;
      if (!childXML->GetScalarAttribute("index", index))
      {
        vtkWarningMacro("Missing 'index' on '" << tagName << "' element in XML. Skipping");
        continue;
      }
      if (index > 0)
      {
        // don't read array selections for partitioned dataset except the first one
        // since that is not expected to change across datasets in a partitioned dataset.
      }
      else
      {
        this->SyncDataArraySelections(this, childXML, filePath);
      }
    }
    else
    {
      vtkErrorMacro("Syntax error in file.");
      return;
    }
  }
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetReader::ReadComposite(vtkXMLDataElement* element,
  vtkCompositeDataSet* composite, const char* filePath, unsigned int& dataSetIndex)
{
  vtkPartitionedDataSet* pds = vtkPartitionedDataSet::SafeDownCast(composite);
  if (!pds)
  {
    vtkErrorMacro("Unsupported composite dataset.");
    return;
  }

  for (int cc = 0; cc < element->GetNumberOfNestedElements(); ++cc)
  {
    vtkXMLDataElement* childXML = element->GetNestedElement(cc);
    if (!childXML || !childXML->GetName())
    {
      continue;
    }
    const char* tagName = childXML->GetName();

    // child is a leaf node, read and insert.
    if (strcmp(tagName, "DataSet") == 0)
    {
      int index = 0;
      if (!childXML->GetScalarAttribute("index", index))
      {
        vtkWarningMacro("Missing 'index' on '" << tagName << "' element in XML. Skipping");
        continue;
      }
      vtkSmartPointer<vtkDataObject> childDS;
      if (this->ShouldReadDataSet(dataSetIndex, index, pds->GetNumberOfPartitions()))
      {
        // Read
        childDS.TakeReference(this->ReadDataObject(childXML, filePath));
      }
      pds->SetPartition(index, childDS);
      dataSetIndex++;
    }
    else
    {
      vtkErrorMacro("Syntax error in file.");
      return;
    }
  }
}
VTK_ABI_NAMESPACE_END
