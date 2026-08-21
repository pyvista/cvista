// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkJoystickFly.h"

#include "vtkCamera.h"
#include "vtkInteractorStyleManipulator.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTimerLog.h"

#include <algorithm>
#include <array>

VTK_ABI_NAMESPACE_BEGIN

//-------------------------------------------------------------------------
vtkJoystickFly::vtkJoystickFly() = default;

//-------------------------------------------------------------------------
vtkJoystickFly::~vtkJoystickFly() = default;

//-------------------------------------------------------------------------
void vtkJoystickFly::OnButtonDown(int, int, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (this->In < 0)
  {
    vtkErrorMacro(
      "Joystick Fly manipulator has to be used from one of the two subclasses (In and Out)");
    return;
  }
  if (!this->HasObserver(vtkJoystickFly::FlyAnimationStepEvent))
  {
    vtkWarningMacro(
      "No observer for FlyAnimationStepEvent event. The fly animation will not work. Please"
      " install an observer for this event. In your observer, call the "
      "`interactor->ProcessEvents()` method"
      " or a suitable method from your GUI framework to process pending events and update the fly "
      "animation.");
    return;
  }
  if (!ren || !rwi)
  {
    vtkErrorMacro("Renderer or Render Window Interactor are not defined");
    return;
  }

  double* range = ren->GetActiveCamera()->GetClippingRange();
  this->Fly(ren, rwi, range[1], (this->In ? 1 : -1) * this->FlySpeed * .01);
}

//-------------------------------------------------------------------------
void vtkJoystickFly::OnButtonUp(int, int, vtkRenderer*, vtkRenderWindowInteractor* rwi)
{
  this->FlyFlag = 0;
  rwi->Render();
}

//-------------------------------------------------------------------------
void vtkJoystickFly::OnMouseMove(int, int, vtkRenderer*, vtkRenderWindowInteractor*) {}

//-------------------------------------------------------------------------
void vtkJoystickFly::Fly(vtkRenderer* ren, vtkRenderWindowInteractor* rwi, double, double ispeed)
{
  if (this->FlyFlag)
  {
    return;
  }
  if (!ren || !rwi)
  {
    vtkErrorMacro("Renderer or Render Window Interactor are not defined");
    return;
  }

  // We'll need the size of the window
  int* size = ren->GetSize();

  // Also we will need the camera.
  vtkCamera* cam = ren->GetActiveCamera();

  // We need to time rendering so that we can adjust the speed
  // accordingly
  vtkNew<vtkTimerLog> timer;
  // We are flying now!
  this->FlyFlag = 1;

  double speed;

  // The first time through we don't want to move
  int first = 1;

  // As long as the mouse is still pressed
  while (this->FlyFlag)
  {
    double* range = cam->GetClippingRange();
    double dist = 0.5 * (range[1] + range[0]);
    double lastx = rwi->GetLastEventPosition()[0];
    double lasty = size[1] - rwi->GetLastEventPosition()[1] - 1;

    // Compute a new render time if appropriate (delta time).
    if (!first)
    {
      // We need at least one time to determine
      // what our increments should be.
      timer->StopTimer();
      this->LastRenderTime = timer->GetElapsedTime();
      // Limit length of render time because we do not want such large jumps
      // when rendering takes more than 1 second.
      this->LastRenderTime = std::min(this->LastRenderTime, 1.0);
    }
    first = 0;

    // Compute angle relative to viewport.
    // These values will be from -0.5 to 0.5
    double vx = (size[0] * 0.5 - lastx) / static_cast<double>(size[0]);
    double vy = (size[1] * 0.5 - lasty) / static_cast<double>(size[0]);

    // Convert to world angle by multiplying by view angle.
    // (Speed up rotation for wide angle views).
    double viewAngle;
    if (cam->GetParallelProjection())
    { // We need to compute a pseudo viewport angle.
      double parallelScale = cam->GetParallelScale();
      viewAngle = 360.0 * atan2(parallelScale * 0.5, dist) / vtkMath::Pi();
    }
    else
    {
      viewAngle = cam->GetViewAngle();
    }
    vx = vx * viewAngle;
    vy = vy * viewAngle;

    // Compute speed.
    speed = ispeed * range[1];

    // Scale speed and rotation by render time
    // to get constant perceived velocities.
    speed = speed * this->LastRenderTime;
    vx = vx * this->LastRenderTime;
    vy = vy * this->LastRenderTime;

    // Start the timer up again
    timer->StartTimer();

    // Do the rotation
    cam->Yaw(vx);
    cam->Pitch(vy);
    cam->OrthogonalizeViewUp();

    // Now figure out if we should slow down our speed because
    // we are trying to make a sharp turn
    vx = (size[0] * 0.5 - lastx) / static_cast<double>(size[0]);
    vy = (size[1] * 0.5 - lasty) / static_cast<double>(size[1]);
    vx = (vx < 0) ? (-vx) : (vx);
    vy = (vy < 0) ? (-vy) : (vy);
    vx = (vx > vy) ? (vx) : (vy);
    speed *= (1.0 - 2.0 * vx);

    // Move the camera forward based on speed.
    // Although this has no effect for parallel projection,
    // it helps keep the pseudo view angle constant.
    std::array<double, 3> focalPoint, position;
    cam->GetPosition(position.data());
    cam->GetFocalPoint(focalPoint.data());
    std::array<double, 3> direction;
    direction[0] = focalPoint[0] - position[0];
    direction[1] = focalPoint[1] - position[1];
    direction[2] = focalPoint[2] - position[2];
    vtkMath::Normalize(direction.data());
    direction[0] *= speed;
    direction[1] *= speed;
    direction[2] *= speed;
    focalPoint[0] += direction[0];
    focalPoint[1] += direction[1];
    focalPoint[2] += direction[2];
    position[0] += direction[0];
    position[1] += direction[1];
    position[2] += direction[2];
    cam->SetPosition(position.data());
    cam->SetFocalPoint(focalPoint.data());

    // In parallel we need to adjust the parallel scale
    if (cam->GetParallelProjection())
    {
      double scale = cam->GetParallelScale();
      if (dist > 0.0 && dist > speed)
      {
        scale = scale * (dist - speed) / dist;
        cam->SetParallelScale(scale);
      }
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

    // Update to process mouse events to get the new position
    // and to check for mouse up events
    this->InvokeEvent(vtkJoystickFly::FlyAnimationStepEvent, rwi);
  }
}

//-------------------------------------------------------------------------
void vtkJoystickFly::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "FlySpeed: " << this->FlySpeed << endl;
}

VTK_ABI_NAMESPACE_END
