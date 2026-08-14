// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCameraManipulator.h"
#include "vtkJoystickFly.h"
#include "vtkRenderWindowInteractor.h"

#include <array>
#include <cmath>
#include <iostream>

inline bool assertTuplesEqual(const std::array<double, 3>& expected,
  const std::array<double, 3>& actual, const char* name, double tolerance = 1e-5)
{
  bool success = true;
  for (int i = 0; i < 3; i++)
  {
    if (std::abs(expected[i] - actual[i]) > tolerance)
    {
      std::cerr << "Mismatch in " << name << "[" << i << "] Expected: " << expected[i]
                << ", Actual: " << actual[i] << std::endl;
      success = false;
    }
  }
  return success;
}

class vtkJoystickFlyObserver : public vtkCommand
{
  int FramesSinceLeftButtonDown = 0;
  int ReleaseLeftButtonAfterFrameCount = -1;

public:
  static vtkJoystickFlyObserver* New() { return new vtkJoystickFlyObserver(); }
  vtkJoystickFlyObserver() = default;
  ~vtkJoystickFlyObserver() override = default;
  vtkTypeMacro(vtkJoystickFlyObserver, vtkCommand);

  int GetFramesSinceLeftButtonDown() const { return this->FramesSinceLeftButtonDown; }
  void ReleaseLeftButtonAfter(int frameCount)
  {
    this->ReleaseLeftButtonAfterFrameCount = frameCount;
  }

  void Execute(vtkObject* vtkNotUsed(caller), unsigned long eventId, void* callData) override
  {
    if (eventId != vtkJoystickFly::FlyAnimationStepEvent)
    {
      return;
    }
    if (auto* interactor =
          vtkRenderWindowInteractor::SafeDownCast(static_cast<vtkObjectBase*>(callData)))
    {
      this->FramesSinceLeftButtonDown++;
      if (this->ReleaseLeftButtonAfterFrameCount > 0 &&
        this->FramesSinceLeftButtonDown >= this->ReleaseLeftButtonAfterFrameCount)
      {
        interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
      }
      interactor->ProcessEvents();
    }
  }
};
