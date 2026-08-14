// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkNanoVDBWriter
 * @brief   NanoVDB writer for vtkImageData
 *
 * Writes a vtkImageData as a NanoVDB file.
 *
 * See https://www.openvdb.org/documentation/doxygen/NanoVDB_MainPage.html for information about the
 * NanoVDB file format and library.
 */

#ifndef vtkNanoVDBWriter_h
#define vtkNanoVDBWriter_h

#include "vtkIONanoVDBModule.h" // needed for exports
#include "vtkWriter.h"

VTK_ABI_NAMESPACE_BEGIN
class vtkImageData;
class vtkInformation;
class vtkMultiProcessController;

class VTKIONANOVDB_EXPORT vtkNanoVDBWriter : public vtkWriter
{
public:
  static vtkNanoVDBWriter* New();
  vtkTypeMacro(vtkNanoVDBWriter, vtkWriter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Get/Set the filename for the file.
   */
  vtkSetFilePathMacro(FileName);
  vtkGetFilePathMacro(FileName);
  ///@}

  ///@{
  /**
   * Get/Set whether or not to save all time steps or just the current time step. Default is false
   * (save only the current time step).
   */
  vtkSetMacro(WriteAllTimeSteps, bool);
  vtkGetMacro(WriteAllTimeSteps, bool);
  ///@}

  ///@{
  /**
   * Get/Set the controller to use. By default,
   * `vtkMultiProcessController::GetGlobalController` will be used.
   */
  void SetController(vtkMultiProcessController*);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);
  ///@}

protected:
  vtkNanoVDBWriter();
  ~vtkNanoVDBWriter() override;

  bool WriteDataAndReturn() override;

  // This writer only accepts vtkImageData input.
  int FillInputPortInformation(int port, vtkInformation* info) override;

  // Needed so we can request pieces and time steps.
  int ProcessRequest(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override;

private:
  vtkNanoVDBWriter(const vtkNanoVDBWriter&) = delete;
  void operator=(const vtkNanoVDBWriter&) = delete;

  bool WriteImageData(vtkImageData* imageData);

  //// State settable through accessors.
  char* FileName = nullptr;
  bool WriteAllTimeSteps = false;

  // The controller for the writer to work in parallel.
  vtkMultiProcessController* Controller = nullptr;

  //// Internal state for tracking time steps.
  int CurrentTimeIndex = 0;
  int NumberOfTimeSteps = 1;
};

VTK_ABI_NAMESPACE_END
#endif
