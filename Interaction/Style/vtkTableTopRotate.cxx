// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTableTopRotate.h"

#include "vtkCamera.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTransform.h"

#include <array>
#include <cmath>
#include <cstdlib>

VTK_ABI_NAMESPACE_BEGIN
namespace
{
void normalizeCameraPosition(vtkCamera* camera, double factor)
{
  std::array<double, 3> position;
  camera->GetPosition(position.data());
  camera->SetPosition(position[0] / factor, position[1] / factor, position[2] / factor);
}

void rescaleCameraPosition(vtkCamera* camera, double factor)
{
  std::array<double, 3> position;
  camera->GetPosition(position.data());
  camera->SetPosition(position[0] * factor, position[1] * factor, position[2] * factor);
}

void normalizeCameraFocalPoint(vtkCamera* camera, double factor)
{
  std::array<double, 3> focalPoint;
  camera->GetFocalPoint(focalPoint.data());
  camera->SetFocalPoint(focalPoint[0] / factor, focalPoint[1] / factor, focalPoint[2] / factor);
}

void rescaleCameraFocalPoint(vtkCamera* camera, double factor)
{
  std::array<double, 3> focalPoint;
  camera->GetFocalPoint(focalPoint.data());
  camera->SetFocalPoint(focalPoint[0] * factor, focalPoint[1] * factor, focalPoint[2] * factor);
}
}

//-------------------------------------------------------------------------
vtkStandardNewMacro(vtkTableTopRotate);

//-------------------------------------------------------------------------
vtkTableTopRotate::vtkTableTopRotate() = default;

//-------------------------------------------------------------------------
vtkTableTopRotate::~vtkTableTopRotate() = default;

//-------------------------------------------------------------------------
void vtkTableTopRotate::OnButtonDown(int, int, vtkRenderer* ren, vtkRenderWindowInteractor*)
{
  this->ComputeCenterOfRotationInDisplayCoordinates(ren);
}

//-------------------------------------------------------------------------
void vtkTableTopRotate::OnButtonUp(int, int, vtkRenderer*, vtkRenderWindowInteractor*) {}

//-------------------------------------------------------------------------
void vtkTableTopRotate::OnMouseMove(int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (ren == nullptr)
  {
    return;
  }
  std::array<double, 3> projectionDirection, viewUp, elevationAxis, oldPosition, newPosition;

  vtkNew<vtkTransform> transform;
  vtkCamera* camera = ren->GetActiveCamera();

  // smooth jerky motion when bounding box exceeds [1e13,1e13,1e13] range
  double scale = vtkMath::Norm(camera->GetPosition());
  if (scale <= 0.0)
  {
    scale = vtkMath::Norm(camera->GetFocalPoint());
    if (scale <= 0.0)
    {
      scale = 1.0;
    }
  }
  normalizeCameraFocalPoint(camera, scale);
  normalizeCameraPosition(camera, scale);

  std::array<double, 3> center;
  this->GetCenterOfRotation(center.data());
  transform->Identity();
  std::transform(
    center.begin(), center.end(), center.begin(), [scale](double v) { return v / scale; });
  transform->Translate(center.data());

  // get azimuth, and elevation deltas in degrees
  const int dx = rwi->GetLastEventPosition()[0] - x;
  const int dy = rwi->GetLastEventPosition()[1] - y;
  const int* size = ren->GetRenderWindow()->GetSize();
  double dAz = dx / static_cast<double>(size[0]) * 180.0;
  double dElev = dy / static_cast<double>(size[1]) * 180.0;

  if (!this->SimultaneouslyAdjustAzimuthElevation)
  {
    if (abs(dx) >= abs(dy))
    {
      dElev = 0.0;
    }
    else
    {
      dAz = 0.0;
    }
  }

  camera->GetDirectionOfProjection(projectionDirection.data());
  vtkMath::Normalize(projectionDirection.data());
  camera->GetViewUp(viewUp.data());
  vtkMath::Normalize(viewUp.data());
  // adjust azimuth
  transform->RotateWXYZ(dAz, viewUp[0], viewUp[1], viewUp[2]);
  // adjust elevation
  const double angle = vtkMath::DegreesFromRadians(acos(vtkMath::Dot(projectionDirection, viewUp)));
  if ((angle + dElev) > 179.0 || (angle + dElev) < 1.0)
  {
    dElev = 0.0;
  }
  vtkMath::Cross(projectionDirection, viewUp, elevationAxis);
  transform->RotateWXYZ(-dElev, elevationAxis.data());
  transform->Translate(-center[0], -center[1], -center[2]);

  camera->GetPosition(oldPosition.data());
  transform->TransformPoint(oldPosition.data(), newPosition.data());
  camera->SetPosition(newPosition.data());

  rescaleCameraPosition(camera, scale);
  rescaleCameraFocalPoint(camera, scale);

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
void vtkTableTopRotate::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "SimultaneouslyAdjustAzimuthElevation: "
     << (this->SimultaneouslyAdjustAzimuthElevation ? "On\n" : "Off\n");
}

VTK_ABI_NAMESPACE_END
