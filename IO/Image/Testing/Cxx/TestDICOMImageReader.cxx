// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// .NAME Test of vtkDICOMImageReader
// .SECTION Description
//

#include "vtkSmartPointer.h"

#include "vtkDICOMImageReader.h"

#include "vtkImageData.h"
#include "vtkImageViewer2.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include <iostream>

int TestDICOMImageReader(int argc, char* argv[])
{

  if (argc <= 1)
  {
    std::cout << "Usage: " << argv[0] << " <dicom file>" << std::endl;
    return 1;
  }

  std::string filename = argv[1];

  vtkSmartPointer<vtkDICOMImageReader> DICOMReader = vtkSmartPointer<vtkDICOMImageReader>::New();

  // Check the image can be read
  if (!DICOMReader->CanReadFile(filename.c_str()))
  {
    std::cerr << "CanReadFile failed for " << filename << "\n";
    exit(1);
  }

  // Read the input image
  DICOMReader->SetFileName(filename.c_str());
  DICOMReader->Update();

  // Read and display the image properties
  const char* fileExtensions = DICOMReader->GetFileExtensions();
  std::cout << "fileExtensions: " << fileExtensions << std::endl;

  const char* descriptiveName = DICOMReader->GetDescriptiveName();
  std::cout << "descriptiveName: " << descriptiveName << std::endl;

  double* pixelSpacing = DICOMReader->GetPixelSpacing();
  std::cout << "pixelSpacing: " << pixelSpacing[0] << ", " << pixelSpacing[1] << std::endl;

  int width = DICOMReader->GetWidth();
  std::cout << "width: " << width << std::endl;

  int height = DICOMReader->GetHeight();
  std::cout << "height: " << height << std::endl;

  float* imagePositionPatient = DICOMReader->GetImagePositionPatient();
  std::cout << "imagePositionPatient: " << imagePositionPatient[0] << ", "
            << imagePositionPatient[1] << ", " << imagePositionPatient[2] << std::endl;

  float* imageOrientationPatient = DICOMReader->GetImageOrientationPatient();
  std::cout << "imageOrientationPatient: " << imageOrientationPatient[0] << ", "
            << imageOrientationPatient[1] << ", " << imageOrientationPatient[2] << ", "
            << imageOrientationPatient[3] << ", " << imageOrientationPatient[4] << ", "
            << imageOrientationPatient[5] << std::endl;

  int bitsAllocated = DICOMReader->GetBitsAllocated();
  std::cout << "bitsAllocated: " << bitsAllocated << std::endl;

  int pixelRepresentation = DICOMReader->GetPixelRepresentation();
  std::cout << "pixelRepresentation: " << pixelRepresentation << std::endl;

  int numberOfComponents = DICOMReader->GetNumberOfComponents();
  std::cout << "numberOfComponents: " << numberOfComponents << std::endl;

  const char* transferSyntaxUID = DICOMReader->GetTransferSyntaxUID();
  std::cout << "transferSyntaxUID: " << transferSyntaxUID << std::endl;

  float rescaleSlope = DICOMReader->GetRescaleSlope();
  std::cout << "rescaleSlope: " << rescaleSlope << std::endl;

  float rescaleOffset = DICOMReader->GetRescaleOffset();
  std::cout << "rescaleOffset: " << rescaleOffset << std::endl;

  const char* patientName = DICOMReader->GetPatientName();
  std::cout << "patientName: " << patientName << std::endl;

  const char* studyUID = DICOMReader->GetStudyUID();
  std::cout << "studyUID: " << studyUID << std::endl;

  const char* studyID = DICOMReader->GetStudyID();
  std::cout << "studyID: " << studyID << std::endl;

  float gantryAngle = DICOMReader->GetGantryAngle();
  std::cout << "gantryAngle: " << gantryAngle << std::endl;

  // Check the metadata
  if (pixelSpacing[0] != 0.9375 || pixelSpacing[1] != 0.9375 ||                   //
    width != 512 || height != 512 ||                                              //
    imagePositionPatient[0] != 0.0 || imagePositionPatient[1] != 0.0 ||           //
    imagePositionPatient[2] != -839.0 ||                                          //
    imageOrientationPatient[0] != 1.0 || imageOrientationPatient[1] != 0.0 ||     //
    imageOrientationPatient[2] != 0.0 || imageOrientationPatient[3] != 0.0 ||     //
    imageOrientationPatient[4] != -1.0 || imageOrientationPatient[5] != 0.0 ||    //
    bitsAllocated != 16 || pixelRepresentation != 0 || numberOfComponents != 1 || //
    strcmp(transferSyntaxUID, "") != 0 ||                                         //
    rescaleSlope != 1.0 || rescaleOffset != -1000.0 ||                            //
    strcmp(patientName, "PRP02022") != 0 ||                                       //
    strcmp(studyUID, "2.16.840.1.113662.2.2397298814474523980922174333") != 0 ||  //
    strcmp(studyID, "   188") != 0 ||                                             //
    gantryAngle != 0.0)
  {
    std::cerr << "ERROR: Incorrect metadata, expected values are:" << std::endl;
    std::cerr << "pixelSpacing: 0.9375, 0.9375" << std::endl;
    std::cerr << "width: 512" << std::endl;
    std::cerr << "height: 512" << std::endl;
    std::cerr << "imagePositionPatient: 0, 0, -839" << std::endl;
    std::cerr << "imageOrientationPatient: 1, 0, 0, 0, -1, 0" << std::endl;
    std::cerr << "bitsAllocated: 16" << std::endl;
    std::cerr << "pixelRepresentation: 0" << std::endl;
    std::cerr << "numberOfComponents: 1" << std::endl;
    std::cerr << "transferSyntaxUID: " << std::endl;
    std::cerr << "rescaleSlope: 1" << std::endl;
    std::cerr << "rescaleOffset: -1000" << std::endl;
    std::cerr << "patientName: PRP02022" << std::endl;
    std::cerr << "studyUID: 2.16.840.1.113662.2.2397298814474523980922174333" << std::endl;
    std::cerr << "studyID:    188" << std::endl;
    std::cerr << "gantryAngle: 0" << std::endl;
    return 1;
  }

  // Display the center slice
  int sliceNumber =
    (DICOMReader->GetOutput()->GetExtent()[5] + DICOMReader->GetOutput()->GetExtent()[4]) / 2;

  // Visualize
  vtkSmartPointer<vtkImageViewer2> imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
  imageViewer->SetInputConnection(DICOMReader->GetOutputPort());
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  imageViewer->SetupInteractor(renderWindowInteractor);
  imageViewer->SetSlice(sliceNumber);
  imageViewer->Render();
  imageViewer->GetRenderer()->ResetCamera();
  renderWindowInteractor->Initialize();
  imageViewer->Render();

  renderWindowInteractor->Start();

  return 0;
}
