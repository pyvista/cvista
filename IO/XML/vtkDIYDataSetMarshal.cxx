// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// vtkDataSet XML (de)serialization for vtkDIYUtilities (VTK::ParallelDIY).
//
// vtkDIYUtilities::Save()/Load() serialize a vtkDataSet to/from a diy buffer as
// an XML string via vtkXMLDataObjectWriter / vtkXMLGenericDataObjectReader. That
// coupled VTK::ParallelDIY to VTK::IOXML, so VTK::ParallelDIY now keeps only a
// runtime hook (RegisterDataSetSerializationHandlers) and the implementation lives
// here, in VTK::IOXML. The static registrar installs the handlers when
// libvtkIOXML is loaded, keeping VTK::ParallelDIY free of any IO dependency while
// preserving identical (LZ4-compressed, non-appended) serialization whenever
// IOXML is present.

#include "vtkDIYUtilities.h"

#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkSmartPointer.h"
#include "vtkXMLDataObjectWriter.h"
#include "vtkXMLGenericDataObjectReader.h"
#include "vtkXMLReader.h"
#include "vtkXMLWriter.h"

#include <string>

namespace
{
//------------------------------------------------------------------------------
int SaveDataSetImpl(vtkDataSet* p, std::string& output)
{
  auto writer = vtkXMLDataObjectWriter::NewWriter(p->GetDataObjectType());
  if (!writer)
  {
    return 0;
  }
  writer->WriteToOutputStringOn();
  writer->SetCompressorTypeToLZ4();
  writer->SetEncodeAppendedData(false);
  writer->SetInputDataObject(p);
  writer->Write();
  output = writer->GetOutputString();
  writer->Delete();
  return 1;
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> LoadDataSetImpl(int type, const std::string& data)
{
  vtkSmartPointer<vtkDataSet> ds;
  if (auto reader = vtkXMLGenericDataObjectReader::CreateReader(type, /*parallel*/ false))
  {
    reader->ReadFromInputStringOn();
    reader->SetInputString(data);
    reader->Update();
    ds = vtkDataSet::SafeDownCast(reader->GetOutputDataObject(0));
  }
  return ds;
}

// Install the handlers when this translation unit's shared library (libvtkIOXML)
// is loaded into the process.
struct DIYMarshalRegistrar
{
  DIYMarshalRegistrar()
  {
    vtkDIYUtilities::RegisterDataSetSerializationHandlers(&SaveDataSetImpl, &LoadDataSetImpl);
  }
};
const DIYMarshalRegistrar vtkIOXMLDIYMarshalRegistrar;
}
