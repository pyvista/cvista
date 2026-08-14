// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkXMLPartitionedDataSetCollectionReader.h"

#include "vtkBase64Utilities.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkCompositeDataSet.h"
#include "vtkDataAssembly.h"
#include "vtkDataAssemblyUtilities.h"
#include "vtkDataSet.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPartitionedDataSetCollection.h"
#include "vtkSmartPointer.h"
#include "vtkXMLDataElement.h"

#include <algorithm>
#include <cctype> // for std::isspace

VTK_ABI_NAMESPACE_BEGIN
namespace
{
vtkSmartPointer<vtkDataAssembly> ReadDataAssembly(
  vtkXMLDataElement* elem, vtkXMLPartitionedDataSetCollectionReader* self)
{
  if (elem->GetAttribute("encoding") == nullptr ||
    strcmp(elem->GetAttribute("encoding"), "base64") != 0 || elem->GetCharacterData() == nullptr)
  {
    vtkWarningWithObjectMacro(self, "Unsupported DataAssembly encoding. Ignoring.");
    return nullptr;
  }

  vtkNew<vtkDataAssembly> assembly;
  const char* encoded_buffer = elem->GetCharacterData();
  size_t len_encoded_data = strlen(encoded_buffer);
  char* decoded_buffer = new char[len_encoded_data];

  // remove leading whitespace, if any.
  while (std::isspace(static_cast<int>(*encoded_buffer)))
  {
    ++encoded_buffer;
    --len_encoded_data;
  }
  auto decoded_buffer_len =
    vtkBase64Utilities::DecodeSafely(reinterpret_cast<const unsigned char*>(encoded_buffer),
      len_encoded_data, reinterpret_cast<unsigned char*>(decoded_buffer), len_encoded_data);
  decoded_buffer[decoded_buffer_len] = '\0';
  assembly->InitializeFromXML(decoded_buffer);
  delete[] decoded_buffer;
  return assembly;
}
}

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkXMLPartitionedDataSetCollectionReader);

//------------------------------------------------------------------------------
vtkXMLPartitionedDataSetCollectionReader::vtkXMLPartitionedDataSetCollectionReader()
{
  this->SetSelector("/"); // Default to read everything and maintain backwards compatibility
}

//------------------------------------------------------------------------------
vtkXMLPartitionedDataSetCollectionReader::~vtkXMLPartitionedDataSetCollectionReader() = default;

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << "Assembly: ";
  if (this->Assembly)
  {
    this->Assembly->PrintSelf(os, indent.GetNextIndent());
  }
  else
  {
    os << "is nullptr" << std::endl;
  }

  os << "AssemblyTag: " << this->AssemblyTag << std::endl;

  os << "Selectors:";
  for (const auto& selector : this->Selectors)
  {
    os << "\n" << selector;
  }
  os << std::endl;
}

//------------------------------------------------------------------------------
int vtkXMLPartitionedDataSetCollectionReader::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPartitionedDataSetCollection");
  return 1;
}

//------------------------------------------------------------------------------
const char* vtkXMLPartitionedDataSetCollectionReader::GetDataSetName()
{
  return "vtkPartitionedDataSetCollection";
}

//----------------------------------------------------------------------------
bool vtkXMLPartitionedDataSetCollectionReader::AddSelector(const char* selector)
{
  if (selector && this->Selectors.insert(selector).second)
  {
    this->Modified();
    return true;
  }

  return false;
}

//----------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::ClearSelectors()
{
  if (!this->Selectors.empty())
  {
    this->Selectors.clear();
    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::SetSelector(const char* selector)
{
  this->ClearSelectors();
  this->AddSelector(selector);
}

//----------------------------------------------------------------------------
int vtkXMLPartitionedDataSetCollectionReader::GetNumberOfSelectors() const
{
  return static_cast<int>(this->Selectors.size());
}

//----------------------------------------------------------------------------
const char* vtkXMLPartitionedDataSetCollectionReader::GetSelector(int index) const
{
  if (index >= 0 && index < this->GetNumberOfSelectors())
  {
    auto iter = std::next(this->Selectors.begin(), index);
    return iter->c_str();
  }
  return nullptr;
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::CreateMetaData(vtkXMLDataElement* ePrimary)
{
  auto pdsc = vtkSmartPointer<vtkPartitionedDataSetCollection>::New();
  for (int cc = 0; cc < ePrimary->GetNumberOfNestedElements(); ++cc)
  {
    vtkXMLDataElement* childXML = ePrimary->GetNestedElement(cc);
    if (!childXML || !childXML->GetName())
    {
      continue;
    }
    const char* tagName = childXML->GetName();
    // child is a partitioned dataset
    if (strcmp(tagName, this->GetXMLPartitionsName()) == 0)
    {
      int index = 0;
      if (!childXML->GetScalarAttribute("index", index))
      {
        vtkWarningMacro("Missing 'index' on '" << tagName << "' element in XML. Skipping");
        continue;
      }
      vtkNew<vtkPartitionedDataSet> childDS;
      pdsc->SetPartitionedDataSet(index, childDS);
      // if XML node has name, set read that.
      if (auto name = childXML->GetAttribute("name"))
      {
        pdsc->GetMetaData(index)->Set(vtkCompositeDataSet::NAME(), name);
      }
      // count how many datasets the partitioned dataset has
      const unsigned int numberOfPartitions =
        vtkXMLCompositeDataReader::CountNestedElements(childXML, "DataSet");
      childDS->SetNumberOfPartitions(numberOfPartitions);
    }
    else if (strcmp(tagName, "DataAssembly") == 0)
    {
      pdsc->SetDataAssembly(::ReadDataAssembly(childXML, this));
    }
    else
    {
      vtkErrorMacro("Syntax error in file.");
      return;
    }
  }

  std::string assemblyXMLContents;
  if (auto assembly = pdsc->GetDataAssembly())
  {
    assemblyXMLContents = assembly->SerializeToXML(vtkIndent());
  }
  else
  {
    vtkNew<vtkDataAssembly> hierarchy;
    vtkDataAssemblyUtilities::GenerateHierarchy(pdsc, hierarchy);
    assemblyXMLContents = hierarchy->SerializeToXML(vtkIndent());
  }
  const std::string existingAssemblyXMLContents = this->Assembly->SerializeToXML(vtkIndent());
  if (existingAssemblyXMLContents != assemblyXMLContents)
  {
    // Assembly has been updated
    this->Assembly->InitializeFromXML(assemblyXMLContents.c_str());
    // Update Assembly widget
    ++this->AssemblyTag;
  }
  this->Metadata = pdsc;
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::SyncCompositeDataArraySelections(
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
    // child is a partitioned dataset
    if (strcmp(tagName, this->GetXMLPartitionsName()) == 0)
    {
      for (int j = 0; j < childXML->GetNumberOfNestedElements(); j++)
      {
        vtkXMLDataElement* datasetXML = childXML->GetNestedElement(j);
        if (!datasetXML || !datasetXML->GetName())
        {
          continue;
        }
        const char* datasetTagName = datasetXML->GetName();
        // child is a leaf node
        if (strcmp(datasetTagName, "DataSet") == 0)
        {
          int index = 0;
          if (!datasetXML->GetScalarAttribute("index", index))
          {
            vtkWarningMacro(
              "Missing 'index' on '" << datasetTagName << "' element in XML. Skipping");
            continue;
          }
          if (index > 0)
          {
            // don't read array selections for partitioned dataset except the first one
            // since that is not expected to change across datasets in a partitioned dataset.
          }
          else
          {
            this->SyncDataArraySelections(this, datasetXML, filePath);
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
bool vtkXMLPartitionedDataSetCollectionReader::IsBlockSelected(unsigned int compositeIndex)
{
  return (this->SelectedCompositeIds.size() == 1 && this->SelectedCompositeIds[0] == 0 /*root*/) ||
    std::find(this->SelectedCompositeIds.begin(), this->SelectedCompositeIds.end(),
      compositeIndex) != this->SelectedCompositeIds.end();
}

//------------------------------------------------------------------------------
bool vtkXMLPartitionedDataSetCollectionReader::CanReadDataObject(vtkDataObject* dataObject)
{
  return dataObject ? !dataObject->IsA("vtkCompositeDataSet") : true;
}

//------------------------------------------------------------------------------
void vtkXMLPartitionedDataSetCollectionReader::ReadComposite(vtkXMLDataElement* element,
  vtkCompositeDataSet* composite, const char* filePath, unsigned int& dataSetIndex)
{
  vtkPartitionedDataSetCollection* pdsc = vtkPartitionedDataSetCollection::SafeDownCast(composite);
  if (!pdsc)
  {
    vtkErrorMacro("Unsupported composite dataset.");
    return;
  }

  this->SelectedCompositeIds = vtkDataAssemblyUtilities::GetSelectedCompositeIds(
    { this->Selectors.begin(), this->Selectors.end() }, this->Assembly, pdsc);

  unsigned int compositeIndex = 0;
  for (int cc = 0; cc < element->GetNumberOfNestedElements(); ++cc)
  {
    vtkXMLDataElement* childXML = element->GetNestedElement(cc);
    if (!childXML || !childXML->GetName())
    {
      continue;
    }
    const char* tagName = childXML->GetName();
    // child is a partitioned dataset
    if (strcmp(tagName, this->GetXMLPartitionsName()) == 0)
    {
      ++compositeIndex;
      int partitionIndex = 0;
      if (!childXML->GetScalarAttribute(this->GetXMLPartitionIndexName(), partitionIndex))
      {
        vtkWarningMacro("Missing '" << this->GetXMLPartitionIndexName() << "' on '" << tagName
                                    << "' element in XML. Skipping");
        continue;
      }
      if (!this->IsBlockSelected(compositeIndex))
      {
        compositeIndex += childXML->GetNumberOfNestedElements();
        continue;
      }
      compositeIndex += childXML->GetNumberOfNestedElements();
      if (auto pds = pdsc->GetPartitionedDataSet(cc))
      {
        for (int j = 0; j < childXML->GetNumberOfNestedElements(); ++j)
        {
          vtkXMLDataElement* datasetXML = childXML->GetNestedElement(j);
          if (!datasetXML || !datasetXML->GetName())
          {
            continue;
          }
          const char* datasetTagName = datasetXML->GetName();
          // child is a leaf node
          if (strcmp(datasetTagName, "DataSet") == 0)
          {
            int index = 0;
            if (!datasetXML->GetScalarAttribute("index", index))
            {
              vtkWarningMacro(
                "Missing 'index' on '" << datasetTagName << "' element in XML. Skipping");
              continue;
            }
            vtkSmartPointer<vtkDataObject> childDS;
            if (this->ShouldReadDataSet(dataSetIndex, index, pds->GetNumberOfPartitions()))
            {
              // Read
              childDS.TakeReference(this->ReadDataObject(datasetXML, filePath));
            }
            if (this->CanReadDataObject(childDS))
            {
              // insert
              pdsc->SetPartition(partitionIndex, index, childDS);
            }
            dataSetIndex++;
          }
        }
      }
    }
  }
}
VTK_ABI_NAMESPACE_END
