// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballZoomToMouse.h"

#include "vtkCamera.h"
#include "vtkInteractorStyle.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

VTK_ABI_NAMESPACE_BEGIN

//-------------------------------------------------------------------------
vtkStandardNewMacro(vtkTrackballZoomToMouse);

//-------------------------------------------------------------------------
vtkTrackballZoomToMouse::vtkTrackballZoomToMouse() = default;

//-------------------------------------------------------------------------
vtkTrackballZoomToMouse::~vtkTrackballZoomToMouse() = default;

//-------------------------------------------------------------------------
void vtkTrackballZoomToMouse::OnButtonDown(
  int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  this->Superclass::OnButtonDown(x, y, ren, rwi);
  rwi->GetEventPosition(this->ZoomPosition);
}

//-------------------------------------------------------------------------
void vtkTrackballZoomToMouse::OnMouseMove(
  int vtkNotUsed(x), int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  const double dy = rwi->GetLastEventPosition()[1] - y;
  // The camera's Dolly function scales by the distance already, so we need
  // to un-scale by it here so the zoom is not too fast or slow.
  const double k = dy * this->ZoomScale / ren->GetActiveCamera()->GetDistance();
  vtkInteractorStyle::DollyToPosition((1.0 + k), this->ZoomPosition, ren);
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
void vtkTrackballZoomToMouse::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ZoomPosition: " << this->ZoomPosition[0] << " " << this->ZoomPosition[1] << endl;
}

VTK_ABI_NAMESPACE_END
