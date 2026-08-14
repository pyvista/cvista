// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkOpenXRRenderWindowInteractor
 * @brief   implements OpenXR specific functions
 * required by vtkRenderWindowInteractor.
 *
 */

#ifndef vtkOpenXRRenderWindowInteractor_h
#define vtkOpenXRRenderWindowInteractor_h

#include "vtkRenderingOpenXRModule.h" // For export macro
#include "vtkVRRenderWindowInteractor.h"

#include "vtkEventData.h" // for ivar

#include <functional> // for std::function
#include <map>        // for std::map
#include <memory>

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGOPENXR_EXPORT vtkOpenXRRenderWindowInteractor : public vtkVRRenderWindowInteractor
{
public:
  static vtkOpenXRRenderWindowInteractor* New();
  vtkTypeMacro(vtkOpenXRRenderWindowInteractor, vtkVRRenderWindowInteractor);

  /**
   * Initialize the event handler
   */
  void Initialize() override;

  void DoOneEvent(vtkVRRenderWindow* renWin, vtkRenderer* ren) override;

  ///@{
  /**
   * Assign an event or std::function to an event path.
   * Called by the interactor style for specific actions
   */
  void AddAction(const std::string& path, const vtkCommand::EventIds&);
  void AddAction(const std::string& path, const std::function<void(vtkEventData*)>&);
  ///@}

  ///@{
  /**
   * Assign an event or std::function to an event path
   *
   * \note
   * The \a isAnalog parameter is ignored; these signatures are intended to satisfy
   * the base class interface and are functionally equivalent to calling the AddAction()
   * function without it.
   */
  void AddAction(const std::string& path, const vtkCommand::EventIds&, bool isAnalog) override;
  void AddAction(
    const std::string& path, bool isAnalog, const std::function<void(vtkEventData*)>&) override;
  ///@}

  /**
   * Apply haptic vibration using the provided action
   * \p action to emit vibration on \p hand to emit on \p amplitude 0.0 to 1.0.
   * \p duration nanoseconds, default 25ms \p frequency (hz)
   */
  bool ApplyVibration(const std::string& actionName, int hand, float amplitude = 0.5,
    float duration = 25000000.0, float frequency = 0.0);

protected:
  /**
   * Create and set the openxr style on this
   * Set ActionManifestFileName to vtk_openxr_actions.json
   * Set ActionSetName to vtk-actions
   */
  vtkOpenXRRenderWindowInteractor();

  ~vtkOpenXRRenderWindowInteractor() override;
  void PrintSelf(ostream& os, vtkIndent indent) override;

  class vtkInternal;
  std::unique_ptr<vtkInternal> Internal;

  vtkNew<vtkMatrix4x4> PoseToWorldMatrix; // used in calculations

private:
  vtkOpenXRRenderWindowInteractor(const vtkOpenXRRenderWindowInteractor&) = delete;
  void operator=(const vtkOpenXRRenderWindowInteractor&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
// VTK-HeaderTest-Exclude: vtkOpenXRRenderWindowInteractor.h
