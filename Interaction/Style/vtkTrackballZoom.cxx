// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballZoom.h"

#include "vtkCamera.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include <array>

VTK_ABI_NAMESPACE_BEGIN

//-------------------------------------------------------------------------
vtkStandardNewMacro(vtkTrackballZoom);

//-------------------------------------------------------------------------
vtkTrackballZoom::vtkTrackballZoom() = default;

//-------------------------------------------------------------------------
vtkTrackballZoom::~vtkTrackballZoom() = default;

//-------------------------------------------------------------------------
void vtkTrackballZoom::OnButtonDown(int, int, vtkRenderer* ren, vtkRenderWindowInteractor*)
{
  int* size = ren->GetSize();
  vtkCamera* camera = ren->GetActiveCamera();

  if (camera->GetParallelProjection() || !this->UseDollyForPerspectiveProjection)
  {
    this->ZoomScale = 1.5 / (double)size[1];
  }
  else
  {
    double* range = camera->GetClippingRange();
    this->ZoomScale = 1.5 * range[1] / (double)size[1];
  }
}

//-------------------------------------------------------------------------
void vtkTrackballZoom::OnMouseMove(
  int vtkNotUsed(x), int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  double dy = rwi->GetLastEventPosition()[1] - y;
  vtkCamera* camera = ren->GetActiveCamera();
  double k;
  std::array<double, 3> directionOfProjection, focalPoint, position;

  if (camera->GetParallelProjection() || !this->UseDollyForPerspectiveProjection)
  {
    k = dy * this->ZoomScale;

    if (camera->GetParallelProjection())
    {
      camera->SetParallelScale((1.0 - k) * camera->GetParallelScale());
    }
    else
    {
      camera->SetViewAngle((1.0 - k) * camera->GetViewAngle());
    }
  }
  else
  {
    camera->GetPosition(position.data());
    camera->GetFocalPoint(focalPoint.data());
    camera->GetDirectionOfProjection(directionOfProjection.data());
    k = dy * this->ZoomScale;

    double tmp = k * directionOfProjection[0];
    position[0] += tmp;
    focalPoint[0] += tmp;

    tmp = k * directionOfProjection[1];
    position[1] += tmp;
    focalPoint[1] += tmp;

    tmp = k * directionOfProjection[2];
    position[2] += tmp;
    focalPoint[2] += tmp;

    if (!camera->GetFreezeFocalPoint())
    {
      camera->SetFocalPoint(focalPoint.data());
    }
    camera->SetPosition(position.data());
  }
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
void vtkTrackballZoom::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ZoomScale: " << this->ZoomScale << endl;
  os << indent << "UseDollyForPerspectiveProjection: " << this->UseDollyForPerspectiveProjection
     << endl;
}

VTK_ABI_NAMESPACE_END
