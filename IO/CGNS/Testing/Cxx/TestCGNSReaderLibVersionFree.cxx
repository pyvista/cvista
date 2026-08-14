// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Menno Deij - van Rijswijk, MARIN, The Netherlands
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkCGNSReader.h"
#include "vtkCell.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkNew.h"
#include "vtkTestUtilities.h"
#include "vtkUnstructuredGrid.h"
#include <vtksys/SystemTools.hxx>

#include <iostream>

// .clang-format off
#include "vtk_cgns.h"
#include VTK_CGNS(cgns_io.h)
#include VTK_CGNS(cgnslib.h)
// .clang-format on

#define vtk_assert(x)                                                                              \
  do                                                                                               \
  {                                                                                                \
    if (!(x))                                                                                      \
    {                                                                                              \
      std::cerr << "On line " << __LINE__ << " ERROR: Condition FAILED!! : " << #x << std::endl;   \
      return EXIT_FAILURE;                                                                         \
    }                                                                                              \
  } while (false)

namespace
{
int ModifyCGNSLibVersion(const std::string& FileName)
{
  int cgioFile;
  int ierr = 0;
  double rootNodeId;
  double childId;
  float FileVersion = 0.0;
  char dataType[CGIO_MAX_DATATYPE_LENGTH + 1];
  char errmsg[CGIO_MAX_ERROR_LENGTH + 1];
  int ndim = 0;
  cgsize_t dimVals[12];
  int fileType = CGIO_FILE_NONE;

  if (cgio_open_file(FileName.c_str(), CGIO_MODE_MODIFY, CGIO_FILE_NONE, &cgioFile) !=
    CGIO_ERR_NONE)
  {
    cgio_error_message(errmsg);
    std::cerr << "Cannot open CGNS file for modification: " << errmsg;
    return 1;
  }

  cgio_get_root_id(cgioFile, &rootNodeId);
  cgio_get_file_type(cgioFile, &fileType);

  if (cgio_get_node_id(cgioFile, rootNodeId, "CGNSLibraryVersion", &childId))
  {
    cgio_error_message(errmsg);
    std::cerr << " CGNSLibraryVersion access error: " << errmsg;
    ierr = 1;
    goto ModifyError;
  }

  if (cgio_get_data_type(cgioFile, childId, dataType))
  {
    std::cerr << "CGNS Version data type";
    ierr = 1;
    goto ModifyError;
  }

  if (cgio_get_dimensions(cgioFile, childId, &ndim, dimVals))
  {
    std::cerr << "cgio_get_dimensions";
    ierr = 1;
    goto ModifyError;
  }

  // check data type
  if (strcmp(dataType, "R4") != 0)
  {
    std::cerr << "Unexpected data type for CGNS-Library-Version=" << dataType;
    ierr = 1;
    goto ModifyError;
  }

  // check data dim
  if ((ndim != 1) || (dimVals[0] != 1))
  {
    std::cerr << "Wrong data dimension for CGNS-Library-Version";
    ierr = 1;
    goto ModifyError;
  }

  // read data
  if (cgio_read_all_data_type(cgioFile, childId, "R4", &FileVersion))
  {
    std::cerr << "read CGNS version number";
    ierr = 1;
    goto ModifyError;
  }

  // Create a fake upgraded library version
  FileVersion = CGNS_DOTVERS + 1.0;

  if (cgio_write_all_data(cgioFile, childId, &FileVersion))
  {
    std::cerr << "Impossible to modify library version number";
    ierr = 1;
    goto ModifyError;
  }

ModifyError:
  cgio_close_file(cgioFile);
  return ierr ? 1 : 0;
}
}

int TestCGNSReaderLibVersionFree(int argc, char* argv[])
{
  char* fname = vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/test_cylinder.cgns");
  std::string originalFile = fname ? fname : "";
  delete[] fname;

  std::string tempDir =
    vtkTestUtilities::GetArgOrEnvOrDefault("-T", argc, argv, "VTK_TEMP_DIR", "Testing/Temporary");
  std::string upgradedFile(tempDir + "/TestUpgrade.cgns");

  if (!vtksys::SystemTools::CopyAFile(originalFile, upgradedFile, true))
  {
    std::cerr << "Failed CopyAFile for testing" << std::endl;
    return EXIT_FAILURE;
  }

  if (::ModifyCGNSLibVersion(upgradedFile))
  {
    return EXIT_FAILURE;
  }

  std::cout << "Opening " << upgradedFile << std::endl;
  vtkNew<vtkCGNSReader> testReader;
  testReader->SetFileName(upgradedFile.c_str());
  testReader->Update();

  std::cout << __FILE__ << " tests passed." << std::endl;
  return EXIT_SUCCESS;
}
