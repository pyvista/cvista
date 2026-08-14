// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebXRRenderer
 * @brief   WebXR renderer
 *
 * vtkWebXRRenderer is a concrete implementation of the abstract class
 * vtkVRRenderer for the WebXR API.
 */

#ifndef vtkWebXRRenderer_h
#define vtkWebXRRenderer_h

#include "vtkRenderingWebXRModule.h" // For export macro
#include "vtkVRRenderer.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGWEBXR_EXPORT VTK_MARSHALAUTO vtkWebXRRenderer : public vtkVRRenderer
{
public:
  static vtkWebXRRenderer* New();
  vtkTypeMacro(vtkWebXRRenderer, vtkVRRenderer);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Create a new Camera suitable for use with this type of Renderer.
   */
  VTK_NEWINSTANCE vtkCamera* MakeCamera() override;

protected:
  vtkWebXRRenderer();
  ~vtkWebXRRenderer() override = default;

private:
  vtkWebXRRenderer(const vtkWebXRRenderer&) = delete;
  void operator=(const vtkWebXRRenderer&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
