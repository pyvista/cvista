// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebXRCamera
 * @brief   WebXR camera
 *
 * vtkWebXRCamera is a concrete implementation of the abstract class
 * vtkVRHMDCamera.
 *
 * vtkWebXRCamera interfaces to the WebXR rendering library.
 *
 * It sets a custom view transform and projection matrix from the view pose and projection
 * given by the WebXR API
 */

#ifndef vtkWebXRCamera_h
#define vtkWebXRCamera_h

#include "vtkRenderingWebXRModule.h" // For export macro
#include "vtkVRHMDCamera.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGWEBXR_EXPORT VTK_MARSHALAUTO vtkWebXRCamera : public vtkVRHMDCamera
{
public:
  static vtkWebXRCamera* New();
  vtkTypeMacro(vtkWebXRCamera, vtkVRHMDCamera);

  /**
   * Renders left and right eyes on the same framebuffer by modifying the viewport and the scissor
   */
  void Render(vtkRenderer* ren) override;

protected:
  vtkWebXRCamera() = default;
  ~vtkWebXRCamera() override = default;

  /**
   * Get the poses for the left and right eyes from the WebXR API
   */
  void UpdateWorldToEyeMatrices(vtkRenderer* ren) override;

  /**
   * Get the projections for the left and right eyes from the WebXR API
   */
  void UpdateEyeToProjectionMatrices(vtkRenderer* ren) override;

private:
  vtkWebXRCamera(const vtkWebXRCamera&) = delete;
  void operator=(const vtkWebXRCamera&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
