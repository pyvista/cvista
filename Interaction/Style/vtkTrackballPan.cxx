// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballPan.h"

#include "vtkCamera.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include <array>

VTK_ABI_NAMESPACE_BEGIN
namespace
{
enum PanAxis
{
  AXIS_X = 0,
  AXIS_Y,
  AXIS_XY
};
}

class vtkTrackballPan::vtkInternals
{
public:
  PanAxis AxisOfMovement = AXIS_XY;
};

vtkStandardNewMacro(vtkTrackballPan);

//-------------------------------------------------------------------------
vtkTrackballPan::vtkTrackballPan()
  : Internals(std::make_unique<vtkTrackballPan::vtkInternals>())
{
}

//-------------------------------------------------------------------------
vtkTrackballPan::~vtkTrackballPan() = default;

//-------------------------------------------------------------------------
void vtkTrackballPan::OnKeyUp(vtkRenderWindowInteractor* interactor)
{
  const auto sym = std::string(interactor->GetKeySym());

  if (sym.find_first_of("xyXY") != std::string::npos)
  {
    this->Internals->AxisOfMovement = AXIS_XY;
  }
}

//-------------------------------------------------------------------------
void vtkTrackballPan::OnKeyDown(vtkRenderWindowInteractor* interactor)
{
  const auto sym = std::string(interactor->GetKeySym());
  if (sym == "x")
  {
    this->Internals->AxisOfMovement = AXIS_X;
  }
  else if (sym == "y")
  {
    this->Internals->AxisOfMovement = AXIS_Y;
  }
}

//-------------------------------------------------------------------------
void vtkTrackballPan::OnButtonDown(int, int, vtkRenderer*, vtkRenderWindowInteractor*) {}

//-------------------------------------------------------------------------
void vtkTrackballPan::OnButtonUp(int, int, vtkRenderer*, vtkRenderWindowInteractor*)
{
  this->Internals->AxisOfMovement = AXIS_XY;
}

//-------------------------------------------------------------------------
void vtkTrackballPan::OnMouseMove(int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (ren == nullptr)
  {
    return;
  }

  // Do not update the Y position (only move X)
  if (this->Internals->AxisOfMovement == AXIS_X)
  {
    y = rwi->GetLastEventPosition()[1];
  }

  // Do not update the X position (only move Y)
  if (this->Internals->AxisOfMovement == AXIS_Y)
  {
    x = rwi->GetLastEventPosition()[0];
  }

  vtkCamera* camera = ren->GetActiveCamera();
  std::array<double, 3> focalPoint, position;
  camera->GetPosition(position.data());
  camera->GetFocalPoint(focalPoint.data());

  if (camera->GetParallelProjection())
  {
    camera->OrthogonalizeViewUp();
    std::array<double, 3> up, vpn;
    camera->GetViewUp(up.data());
    camera->GetViewPlaneNormal(vpn.data());
    std::array<double, 3> right;
    vtkMath::Cross(vpn, up, right);

    double scale, tmp;
    scale = camera->GetParallelScale();
    // These are different because y is flipped.
    const int* size = ren->GetSize();
    const double dx = scale * 2.0 * (x - rwi->GetLastEventPosition()[0]) / (size[1]);
    const double dy = scale * 2.0 * (rwi->GetLastEventPosition()[1] - y) / (size[1]);

    tmp = (right[0] * dx + up[0] * dy);
    position[0] += tmp;
    focalPoint[0] += tmp;
    tmp = (right[1] * dx + up[1] * dy);
    position[1] += tmp;
    focalPoint[1] += tmp;
    tmp = (right[2] * dx + up[2] * dy);
    position[2] += tmp;
    focalPoint[2] += tmp;
    camera->SetPosition(position.data());
    camera->SetFocalPoint(focalPoint.data());
  }
  else
  {
    double depth;
    std::array<double, 4> worldPt, lastWorldPt;

    std::array<double, 3> center;
    this->GetCenterOfRotation(center.data());
    ren->SetWorldPoint(center[0], center[1], center[2], 1.0);

    ren->WorldToDisplay();
    depth = ren->GetDisplayPoint()[2];

    ren->SetDisplayPoint(x, y, depth);
    ren->DisplayToWorld();
    ren->GetWorldPoint(worldPt.data());
    if (worldPt[3])
    {
      worldPt[0] /= worldPt[3];
      worldPt[1] /= worldPt[3];
      worldPt[2] /= worldPt[3];
      worldPt[3] = 1.0;
    }

    ren->SetDisplayPoint(rwi->GetLastEventPosition()[0], rwi->GetLastEventPosition()[1], depth);
    ren->DisplayToWorld();
    ren->GetWorldPoint(lastWorldPt.data());
    if (lastWorldPt[3])
    {
      lastWorldPt[0] /= lastWorldPt[3];
      lastWorldPt[1] /= lastWorldPt[3];
      lastWorldPt[2] /= lastWorldPt[3];
      lastWorldPt[3] = 1.0;
    }

    position[0] += lastWorldPt[0] - worldPt[0];
    position[1] += lastWorldPt[1] - worldPt[1];
    position[2] += lastWorldPt[2] - worldPt[2];

    focalPoint[0] += lastWorldPt[0] - worldPt[0];
    focalPoint[1] += lastWorldPt[1] - worldPt[1];
    focalPoint[2] += lastWorldPt[2] - worldPt[2];

    camera->SetPosition(position.data());
    camera->SetFocalPoint(focalPoint.data());
  }
  ren->ResetCameraClippingRange();
  rwi->Render();
}

//-------------------------------------------------------------------------
void vtkTrackballPan::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

VTK_ABI_NAMESPACE_END
