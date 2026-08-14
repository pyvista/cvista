// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
* @class   vtkWebXRModel
* @brief   WebXR device model

* This internal class is used to load models
* such as for the trackers and controllers and to
* render them in the scene
*/

#ifndef vtkWebXRModel_h
#define vtkWebXRModel_h

#include "vtkRenderingWebXRModule.h" // For export macro
#include "vtkVRModel.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

#include <memory> // for std::unique_ptr

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGWEBXR_EXPORT VTK_MARSHALAUTO vtkWebXRModel : public vtkVRModel
{
public:
  static vtkWebXRModel* New();
  vtkTypeMacro(vtkWebXRModel, vtkVRModel);

protected:
  vtkWebXRModel();
  ~vtkWebXRModel() override;

  /**
   * Upload the model's mesh to the GPU
   */
  void FillModelHelper() override;

  /**
   * Setup the position array
   */
  void SetPositionAndTCoords() override;

  /**
   * Create empty texture
   */
  void CreateTextureObject(vtkOpenGLRenderWindow* win) override;

  /**
   * Create a green pyramid as a controller model
   */
  void LoadModelAndTexture(vtkOpenGLRenderWindow* win) override;

private:
  vtkWebXRModel(const vtkWebXRModel&) = delete;
  void operator=(const vtkWebXRModel&) = delete;

  std::string AssetPath;

  class vtkInternals;
  std::unique_ptr<vtkInternals> Internal;
};

VTK_ABI_NAMESPACE_END
#endif
