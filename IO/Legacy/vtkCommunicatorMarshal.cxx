// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Data-object (de)serialization for vtkCommunicator (VTK::ParallelCore).
//
// vtkCommunicator::MarshalDataObject()/UnMarshalDataObject() serialize a data
// object to/from a byte buffer using the legacy vtkGenericDataObjectWriter /
// vtkGenericDataObjectReader. That couples the serialization to VTK::IOLegacy,
// so VTK::ParallelCore keeps only a runtime hook (RegisterMarshalDataObjectHandlers)
// and the actual implementation lives here, in VTK::IOLegacy, where the legacy
// reader/writer already reside. The static registrar below installs the handlers
// when libvtkIOLegacy is loaded, keeping VTK::ParallelCore free of any IO
// dependency while preserving identical behavior whenever IOLegacy is present.

#include "vtkCommunicator.h"

#include "vtkCharArray.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkGenericDataObjectReader.h"
#include "vtkGenericDataObjectWriter.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkRectilinearGrid.h"
#include "vtkSmartPointer.h"
#include "vtkStringFormatter.h"
#include "vtkStringScanner.h"
#include "vtkStructuredData.h" // for VTK_3D_EXTENT
#include "vtkStructuredGrid.h"

#include <cstring>
#include <string_view>
#include <tuple>

namespace
{
constexpr int EXTENT_HEADER_SIZE = 128;

//------------------------------------------------------------------------------
int MarshalDataObjectImpl(vtkDataObject* object, vtkCharArray* buffer)
{
  buffer->Initialize();
  buffer->SetNumberOfComponents(1);

  if (object == nullptr)
  {
    buffer->SetNumberOfTuples(0);
    return 1;
  }

  vtkNew<vtkGenericDataObjectWriter> writer;

  vtkSmartPointer<vtkDataObject> copy;
  copy.TakeReference(object->NewInstance());
  copy->ShallowCopy(object);

  writer->SetFileTypeToBinary();
  // There is a problem with binary files with no data.
  if (vtkDataSet::SafeDownCast(copy) != nullptr)
  {
    vtkDataSet* ds = vtkDataSet::SafeDownCast(copy);
    if (ds->GetNumberOfCells() + ds->GetNumberOfPoints() == 0)
    {
      writer->SetFileTypeToASCII();
    }
  }
  writer->WriteToOutputStringOn();
  writer->SetInputData(copy);

  if (!writer->Write())
  {
    vtkGenericWarningMacro("Error detected while marshaling data object.");
    return 0;
  }
  const vtkIdType size = writer->GetOutputStringLength();
  if (object->GetExtentType() == VTK_3D_EXTENT)
  {
    // You would think that the extent information would be properly saved, but
    // no, it is not.
    int extent[6] = { 0, 0, 0, 0, 0, 0 };
    vtkRectilinearGrid* rg = vtkRectilinearGrid::SafeDownCast(object);
    vtkStructuredGrid* sg = vtkStructuredGrid::SafeDownCast(object);
    vtkImageData* id = vtkImageData::SafeDownCast(object);
    if (rg)
    {
      rg->GetExtent(extent);
    }
    else if (sg)
    {
      sg->GetExtent(extent);
    }
    else if (id)
    {
      id->GetExtent(extent);
    }
    char extentHeader[EXTENT_HEADER_SIZE];
    auto result =
      vtk::format_to_n(extentHeader, sizeof(extentHeader), "EXTENT {:d} {:d} {:d} {:d} {:d} {:d}",
        extent[0], extent[1], extent[2], extent[3], extent[4], extent[5]);
    *result.out = '\0';

    buffer->SetNumberOfTuples(size + EXTENT_HEADER_SIZE);
    memcpy(buffer->GetPointer(0), extentHeader, EXTENT_HEADER_SIZE);
    memcpy(buffer->GetPointer(EXTENT_HEADER_SIZE), writer->GetOutputString(), size);
  }
  else
  {
    buffer->SetArray(
      writer->RegisterAndGetOutputString(), size, 0, vtkCharArray::VTK_DATA_ARRAY_DELETE);
    buffer->SetNumberOfTuples(size);
  }
  return 1;
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkDataObject> UnMarshalDataObjectImpl(vtkCharArray* buffer)
{
  vtkIdType bufferSize = buffer ? buffer->GetNumberOfTuples() : 0;
  if (bufferSize <= 0)
  {
    return nullptr;
  }

  // You would think that the extent information would be properly saved, but
  // no, it is not.
  int extent[6] = { 0, 0, 0, 0, 0, 0 };
  std::string_view bufferView(buffer->GetPointer(0), bufferSize);
  if (bufferView.substr(0, 6) == "EXTENT")
  {
    auto result =
      vtk::scan<int, int, int, int, int, int>(bufferView, "EXTENT {:d} {:d} {:d} {:d} {:d} {:d}");
    std::tie(extent[0], extent[1], extent[2], extent[3], extent[4], extent[5]) = result->values();
    bufferView = bufferView.substr(EXTENT_HEADER_SIZE);
    bufferSize -= EXTENT_HEADER_SIZE;
  }

  // Make a temporary array object holding the part of the buffer that can be
  // parsed by the reader.
  vtkNew<vtkCharArray> objectBuffer;
  objectBuffer->SetNumberOfComponents(1);
  objectBuffer->SetArray(const_cast<char*>(bufferView.data()), bufferSize, 1);

  vtkNew<vtkGenericDataObjectReader> reader;
  reader->ReadFromInputStringOn();
  reader->SetInputArray(objectBuffer);
  reader->Update();

  vtkSmartPointer<vtkDataObject> dobj = reader->GetOutputDataObject(0);
  if (dobj->GetExtentType() == VTK_3D_EXTENT)
  {
    if (vtkRectilinearGrid* rg = vtkRectilinearGrid::SafeDownCast(dobj))
    {
      rg->SetExtent(extent);
    }
    else if (vtkStructuredGrid* sg = vtkStructuredGrid::SafeDownCast(dobj))
    {
      sg->SetExtent(extent);
    }
    else if (vtkImageData* id = vtkImageData::SafeDownCast(dobj))
    {
      // If we fix the extent, we need to fix the origin too.
      double origin[3];
      id->GetOrigin(origin);
      double spacing[3];
      id->GetSpacing(spacing);
      int readerExt[6];
      id->GetExtent(readerExt);
      for (int i = 0; i < 3; i++)
      {
        if (readerExt[2 * i] != extent[2 * i])
        {
          origin[i] = origin[i] - (extent[2 * i] - readerExt[2 * i]) * spacing[i];
        }
      }
      id->SetExtent(extent);
      id->SetOrigin(origin);
    }
  }
  return dobj;
}

// Install the handlers when this translation unit's shared library (libvtkIOLegacy)
// is loaded into the process.
struct MarshalRegistrar
{
  MarshalRegistrar()
  {
    vtkCommunicator::RegisterMarshalDataObjectHandlers(
      &MarshalDataObjectImpl, &UnMarshalDataObjectImpl);
  }
};
const MarshalRegistrar vtkIOLegacyMarshalRegistrar;
}
