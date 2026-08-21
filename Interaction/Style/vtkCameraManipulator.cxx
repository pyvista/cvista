// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkCameraManipulator.h"

#include "vtkObjectFactory.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

VTK_ABI_NAMESPACE_BEGIN
namespace
{
const char* GetMouseButtonAsString(vtkCameraManipulator::MouseButtonType button)
{
  switch (button)
  {
    case vtkCameraManipulator::MouseButtonType::Left:
      return "Left";
    case vtkCameraManipulator::MouseButtonType::Middle:
      return "Middle";
    case vtkCameraManipulator::MouseButtonType::Right:
      return "Right";
    default:
      return "Unknown";
  }
}
}

//-------------------------------------------------------------------------
vtkCameraManipulator::vtkCameraManipulator() = default;

//-------------------------------------------------------------------------
vtkCameraManipulator::~vtkCameraManipulator()
{
  this->SetManipulatorName(nullptr);
}

//-------------------------------------------------------------------------
void vtkCameraManipulator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "ManipulatorName: " << this->ManipulatorName << '\n';
  os << indent << "MouseButton: " << GetMouseButtonAsString(this->MouseButton) << '\n';
  os << indent << "Alt: " << this->Alt << '\n';
  os << indent << "Control: " << this->Control << '\n';
  os << indent << "Shift: " << this->Shift << '\n';
  os << indent << "CenterOfRotation: " << this->CenterOfRotation[0] << ", "
     << this->CenterOfRotation[1] << ", " << this->CenterOfRotation[2] << '\n';
  os << indent << "MouseMotionFactor: " << this->MouseMotionFactor << '\n';
}

//-------------------------------------------------------------------------
void vtkCameraManipulator::ComputeCenterOfRotationInDisplayCoordinates(vtkRenderer* ren)
{
  // save the center of rotation in screen coordinates
  ren->SetWorldPoint(
    this->CenterOfRotation[0], this->CenterOfRotation[1], this->CenterOfRotation[2], 1.0);
  ren->WorldToDisplay();
  // GetDisplayPoint expects a pointer to an array of 3 doubles, but we only care about the first 2
  std::array<double, 3> displayPoint;
  ren->GetDisplayPoint(displayPoint.data());
  this->CenterOfRotationInDisplayCoordinates[0] = displayPoint[0];
  this->CenterOfRotationInDisplayCoordinates[1] = displayPoint[1];
}
VTK_ABI_NAMESPACE_END
