// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkExecutive.h"
#include "vtkMemoryResourceStream.h"
#include "vtkTGAReader.h"

#include "vtkTestErrorObserver.h"
#include "vtkTestUtilities.h"

#include <iostream>

int TestTGAReaderInvalidFileHandling(int argc, char* argv[])
{
  if (argc <= 1)
  {
    std::cout << "Usage: " << argv[0] << " <valid tga file>" << std::endl;
    return EXIT_FAILURE;
  }

  std::string filename = argv[1];

  auto testData = [](const std::vector<unsigned char>& vec, const std::string& expectedMsg)
  {
    // Convert vector to stream
    vtkNew<vtkMemoryResourceStream> stream;
    stream->SetBuffer(vec.data(), vec.size());

    vtkNew<vtkTest::ErrorObserver> errorObserver;
    vtkNew<vtkTest::ErrorObserver> errorObserverExec;

    // Initialize and update reader with errorObservers
    vtkNew<vtkTGAReader> tgaReader;
    tgaReader->AddObserver(vtkCommand::ErrorEvent, errorObserver);
    tgaReader->GetExecutive()->AddObserver(vtkCommand::ErrorEvent, errorObserverExec);
    tgaReader->SetStream(stream);
    tgaReader->Update();

    auto result = errorObserver->CheckErrorMessage(expectedMsg);
    if (result != 0)
    {
      std::cout << "Error message : {\n"
                << errorObserver->GetErrorMessage() << "\n} does not match expected\n"
                << std::endl;
    }
    errorObserver->Clear();

    return result;
  };

  // Undersized header : file has less than the minimum 18 bytes for a TGA header
  std::vector<unsigned char> undersizedHeader = { 0x00, 0x00, 10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 32 };
  // Invalid header file : file with a header that is unsupported by VTK (in this case the "format"
  // byte is set to 0xFF)
  std::vector<unsigned char> headerInvalidFile = { 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 32, 0x00 };

  // Partial file : a normal TGA file (Data/vtk.tga) which has had a random amount data trimmed off
  // of the end (100 bytes)

  // Open test TGA file
  std::ifstream file(filename, std::ios::binary);
  if (!file)
  {
    std::cerr << "Could not open file " << filename << std::endl;
  }

  // Read test file into vector
  std::vector<unsigned char> partialFile(
    (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  // Trim random amount of bytes off of end of vector
  partialFile.resize(partialFile.size() - 100);

  // Run tests

  if (testData(undersizedHeader, "TGAReader error reading file: Invalid header.") != 0)
  {
    std::cerr << "Undersized header test failed\n";
    return EXIT_FAILURE;
  }

  if (testData(headerInvalidFile, "TGAReader error reading file: Invalid header.") != 0)
  {
    std::cerr << "Invalid header test failed\n";
    return EXIT_FAILURE;
  }

  if (testData(partialFile, "TGAReader error reading file: Premature EOF while reading.") != 0)
  {
    std::cerr << "Partial file test failed\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
