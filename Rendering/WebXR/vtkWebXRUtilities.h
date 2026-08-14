// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebXRUtilities
 * @brief   Header file that contains utility functions for WebXR
 *
 * This class contains inline functions to create matrices from WebXR pose.
 */

#ifndef vtkWebXRUtilities_h
#define vtkWebXRUtilities_h

#include "vtkEventData.h"
#include "vtkMatrix4x4.h"
#include "vtkWebXR.h"

VTK_ABI_NAMESPACE_BEGIN
namespace vtkWebXRUtilities
{

//----------------------------------------------------------------------------
// transpose of VTK standard
inline void SetMatrixFromWebXRMatrix(vtkMatrix4x4* result, const double* webXRMatrix)
{
  result->SetData(webXRMatrix);
  result->Transpose();
  result->Modified();
}

//------------------------------------------------------------------------------
inline vtkEventDataDeviceInput GetInputFromWebXRButton(WebXRGamepadButtonMapping button)
{
  switch (button)
  {
    case WEBXR_BUTTON_PRIMARY_TRIGGER:
    {
      return vtkEventDataDeviceInput::Trigger;
    }
    case WEBXR_BUTTON_PRIMARY_TOUCHPAD:
    {
      return vtkEventDataDeviceInput::TrackPad;
    }
    case WEBXR_BUTTON_PRIMARY_SQUEEZE:
    {
      return vtkEventDataDeviceInput::Grip;
    }
    case WEBXR_BUTTON_PRIMARY_THUMBSTICK:
    {
      return vtkEventDataDeviceInput::Joystick;
    }
    case WEBXR_BUTTON_1:
    case WEBXR_BUTTON_2:
    case WEBXR_BUTTON_3:
    case WEBXR_BUTTON_4:
    {
      return vtkEventDataDeviceInput::Any;
    }
    default:
    {
      return vtkEventDataDeviceInput::Unknown;
    }
  }
}

//------------------------------------------------------------------------------
inline vtkEventDataDevice GetDeviceFromWebXRHand(WebXRHandedness hand)
{
  switch (hand)
  {
    case WEBXR_HANDEDNESS_LEFT:
    {
      return vtkEventDataDevice::LeftController;
    }
    case WEBXR_HANDEDNESS_RIGHT:
    {
      return vtkEventDataDevice::RightController;
    }
    case WEBXR_HANDEDNESS_NONE:
    default:
    {
      return vtkEventDataDevice::Unknown;
    }
  }
}

} // namespace vtkWebXRUtilities

VTK_ABI_NAMESPACE_END
#endif
// VTK-HeaderTest-Exclude: vtkWebXRUtilities.h
