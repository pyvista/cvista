// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballRoll.h"

#include "vtkCamera.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTransform.h"

#include <array>

VTK_ABI_NAMESPACE_BEGIN

//-------------------------------------------------------------------------
vtkStandardNewMacro(vtkTrackballRoll);

//-------------------------------------------------------------------------
vtkTrackballRoll::vtkTrackballRoll() = default;

//-------------------------------------------------------------------------
vtkTrackballRoll::~vtkTrackballRoll() = default;

//-------------------------------------------------------------------------
void vtkTrackballRoll::OnButtonDown(int, int, vtkRenderer*, vtkRenderWindowInteractor*) {}

//-------------------------------------------------------------------------
void vtkTrackballRoll::OnButtonUp(int, int, vtkRenderer*, vtkRenderWindowInteractor*) {}

//-------------------------------------------------------------------------
void vtkTrackballRoll::OnMouseMove(int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (ren == nullptr)
  {
    return;
  }

  vtkCamera* camera = ren->GetActiveCamera();
  double axis[3];

  // compute view vector (rotation axis)
  std::array<double, 3> focalPoint, position;
  camera->GetPosition(position.data());
  camera->GetFocalPoint(focalPoint.data());

  axis[0] = focalPoint[0] - position[0];
  axis[1] = focalPoint[1] - position[1];
  axis[2] = focalPoint[2] - position[2];

  // compute the angle of rotation
  // - first compute the two vectors (center to mouse)
  this->ComputeCenterOfRotationInDisplayCoordinates(ren);

  std::array<int, 2> lastEventPos;
  rwi->GetLastEventPosition(lastEventPos.data());
  int x1, x2, y1, y2;
  x1 = lastEventPos[0] - static_cast<int>(this->CenterOfRotationInDisplayCoordinates[0]);
  x2 = x - static_cast<int>(this->CenterOfRotationInDisplayCoordinates[0]);
  y1 = lastEventPos[1] - static_cast<int>(this->CenterOfRotationInDisplayCoordinates[1]);
  y2 = y - static_cast<int>(this->CenterOfRotationInDisplayCoordinates[1]);
  const double denom =
    sqrt(static_cast<double>(x1 * x1 + y1 * y1)) * sqrt(static_cast<double>(x2 * x2 + y2 * y2));
  if (denom == 0.0)
  {
    // don't ever want to divide by zero
    return;
  }

  // - divide by magnitudes to get angle
  const double angle = vtkMath::DegreesFromRadians((x1 * y2 - y1 * x2) / denom);
  // translate to center
  vtkNew<vtkTransform> transform;
  transform->Identity();
  std::array<double, 3> center;
  this->GetCenterOfRotation(center.data());
  transform->Translate(center[0], center[1], center[2]);

  // roll
  transform->RotateWXYZ(angle, axis[0], axis[1], axis[2]);

  // translate back
  transform->Translate(-center[0], -center[1], -center[2]);

  camera->ApplyTransform(transform);
  camera->OrthogonalizeViewUp();

  if (auto* styleManipulator =
        vtkInteractorStyleManipulator::SafeDownCast(rwi->GetInteractorStyle()))
  {
    if (styleManipulator->GetAutoAdjustCameraClippingRange())
    {
      ren->ResetCameraClippingRange();
    }
  }
  rwi->Render();
}

//-------------------------------------------------------------------------
void vtkTrackballRoll::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

VTK_ABI_NAMESPACE_END
