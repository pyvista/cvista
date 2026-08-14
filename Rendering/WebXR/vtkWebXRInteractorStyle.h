// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebXRInteractorStyle
 * @brief   extended from vtkInteractorStyle3D to override command methods
 */

#ifndef vtkWebXRInteractorStyle_h
#define vtkWebXRInteractorStyle_h

#include "vtkRenderingWebXRModule.h" // For export macro
#include "vtkVRInteractorStyle.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGWEBXR_EXPORT VTK_MARSHALAUTO vtkWebXRInteractorStyle : public vtkVRInteractorStyle
{
public:
  static vtkWebXRInteractorStyle* New();
  vtkTypeMacro(vtkWebXRInteractorStyle, vtkVRInteractorStyle);

  /**
   * Setup default actions defined with an action path and a corresponding command.
   */
  void SetupActions(vtkRenderWindowInteractor* iren) override;

  /**
   * Creates a new ControlsHelper suitable for use with this class.
   * Not implemented as the WebXR Module cannot show the controller models yet
   */
  vtkVRControlsHelper* MakeControlsHelper() override { return nullptr; };

  /**
   * no-op to satisfy the base class API
   */
  void LoadNextCameraPose() override {}

protected:
  vtkWebXRInteractorStyle() = default;
  ~vtkWebXRInteractorStyle() override = default;

private:
  vtkWebXRInteractorStyle(const vtkWebXRInteractorStyle&) = delete;
  void operator=(const vtkWebXRInteractorStyle&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
