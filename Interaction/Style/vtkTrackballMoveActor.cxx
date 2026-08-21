// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballMoveActor.h"

#include "vtkInteractorStyleManipulator.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include <array>

VTK_ABI_NAMESPACE_BEGIN

vtkStandardNewMacro(vtkTrackballMoveActor);

//-------------------------------------------------------------------------
vtkTrackballMoveActor::vtkTrackballMoveActor() = default;

//-------------------------------------------------------------------------
vtkTrackballMoveActor::~vtkTrackballMoveActor() = default;

//-------------------------------------------------------------------------
void vtkTrackballMoveActor::OnButtonDown(int, int, vtkRenderer*, vtkRenderWindowInteractor*)
{
  if (!this->HasObserver(RequestActiveSourceBoundsEvent) ||
    !this->HasObserver(RequestActiveSourcePositionEvent) ||
    !this->HasObserver(ApplyActiveSourcePositionEvent))
  {
    vtkErrorMacro("vtkTrackballMoveActor requires observers for RequestActiveSourceBoundsEvent, "
                  "RequestActiveSourcePositionEvent, and ApplyActiveSourcePositionEvent events.");
  }
}

//-------------------------------------------------------------------------
void vtkTrackballMoveActor::OnMouseMove(
  int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (ren == nullptr)
  {
    return;
  }

  // These are different because y is flipped.

  double bounds[6];
  this->InvokeEvent(RequestActiveSourceBoundsEvent, bounds);
  // Get bounds
  if (vtkMath::AreBoundsInitialized(bounds))
  {
    std::array<double, 4> center;
    std::array<double, 3> dpoint1;
    std::array<double, 4> startpoint;
    std::array<double, 4> endpoint;
    int cc;

    // Calculate center of bounds.
    for (cc = 0; cc < 3; cc++)
    {
      center[cc] = (bounds[cc * 2] + bounds[cc * 2 + 1]) / 2;
    }
    center[3] = 1;

    // Convert the center of bounds to display coordinate
    ren->SetWorldPoint(center.data());
    ren->WorldToDisplay();
    ren->GetDisplayPoint(dpoint1.data());

    // Convert start point to world coordinate
    ren->SetDisplayPoint(
      rwi->GetLastEventPosition()[0], rwi->GetLastEventPosition()[1], dpoint1[2]);
    ren->DisplayToWorld();
    ren->GetWorldPoint(startpoint.data());

    // Convert end point to world coordinate
    ren->SetDisplayPoint(x, y, dpoint1[2]);
    ren->DisplayToWorld();
    ren->GetWorldPoint(endpoint.data());

    for (cc = 0; cc < 3; cc++)
    {
      startpoint[cc] /= startpoint[3];
      endpoint[cc] /= endpoint[3];
    }

    std::array<double, 3> position;
    this->InvokeEvent(RequestActiveSourcePositionEvent, position.data());
    for (cc = 0; cc < 3; cc++)
    {
      position[cc] += endpoint[cc] - startpoint[cc];
    }
    this->InvokeEvent(ApplyActiveSourcePositionEvent, position.data());

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
}

//-------------------------------------------------------------------------
void vtkTrackballMoveActor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
