// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebXRRenderWindowInteractor
 * @brief   implements WebXR specific functions required by vtkVRRenderWindowInteractor.
 *
 * This class uses the WebXR API to retrieve XR controller information.
 */

#ifndef vtkWebXRRenderWindowInteractor_h
#define vtkWebXRRenderWindowInteractor_h

#include "vtkRenderingWebXRModule.h" // For export macro
#include "vtkVRRenderWindowInteractor.h"
#include "vtkWebXR.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN

class VTKRENDERINGWEBXR_EXPORT VTK_MARSHALAUTO vtkWebXRRenderWindowInteractor
  : public vtkVRRenderWindowInteractor
{
public:
  vtkTypeMacro(vtkWebXRRenderWindowInteractor, vtkVRRenderWindowInteractor);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  static vtkWebXRRenderWindowInteractor* New();

  enum class SessionMode
  {
    VR = WEBXR_SESSION_MODE_IMMERSIVE_VR,
    AR = WEBXR_SESSION_MODE_IMMERSIVE_AR,
    INLINE = WEBXR_SESSION_MODE_INLINE,
  };

  /**
   * Initialize the event handler
   */
  void Initialize() override;

  /**
   * Start emscripten main loop with an empty function
   */
  void StartEventLoop() override;

  /**
   * Implement the event loop
   */
  void DoOneEvent(vtkVRRenderWindow* renWin, vtkRenderer* ren) override;

  /**
   * Stop the XR session and stop the emscripten main loop
   */
  void TerminateApp() override;

  /**
   * Retrieve camera's world coordinates from WebXR API's pose
   * @param xrPose WebXR pose
   * @param pos Output world position
   * @param wxyz Output world orientation quaternion
   * @param ppos Output physical position
   * @param wdir Output world view direction (-Z)
   */
  void ConvertWebXRPoseToWorldCoordinates(const WebXRRigidTransform& xrPose, double pos[3],
    double wxyz[4], double ppos[3], double wdir[3]);

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
   * Updates WebXR controllers state and invoke corresponding events
   */
  void UpdateXRControllers();

  ///@{
  /**
   * Starts and Stops a WebXR Session.
   *
   * Default mode is VR and required feature "local", with optional feature "local-floor"
   *
   * \warning
   * Must be called only by the vtkWasmSceneManager or a custom app
   */
  static void StartXR(SessionMode mode = SessionMode::VR, vtkTypeUInt32 requiredFeatures = 1,
    vtkTypeUInt32 optionalFeatures = 2);
  static void StopXR();
  ///@}

protected:
  vtkWebXRRenderWindowInteractor();
  ~vtkWebXRRenderWindowInteractor() override;

  class ActionData
  {
  public:
    vtkCommand::EventIds EventId;
    vtkEventDataDeviceInput DeviceInput = vtkEventDataDeviceInput::Unknown;
    std::function<void(vtkEventData*)> Function;
    bool UseFunction = false;
    bool IsHandPose = false;
  };

  /**
   * Load JSON actions file from given path for interactions
   */
  bool LoadActions(const std::string& actionFilename);
  const ActionData* GetActionData(const std::string& actionName);
  const ActionData* GetActionData(
    WebXRHandedness hand, WebXRGamepadButtonMapping button, WebXRInputPoseMode mode);

  /**
   * Invoke event or callback of an action
   */
  void ApplyAction(const ActionData& actionData, vtkEventDataDevice3D* ed);

  std::map<std::string, ActionData> ActionMap;
  struct WebXRInput
  {
    WebXRHandedness hand;
    WebXRGamepadButtonMapping button;
    WebXRInputPoseMode mode;

    bool operator<(const WebXRInput& other) const
    {
      if (this->hand != other.hand)
      {
        return this->hand < other.hand;
      }
      else if (this->button != other.button)
      {
        return this->button < other.button;
      }
      else
      {
        return this->mode < other.mode;
      }
    }
  };
  std::map<WebXRInput, ActionData*> ButtonToActionMap;

private:
  vtkWebXRRenderWindowInteractor(const vtkWebXRRenderWindowInteractor&) = delete;
  void operator=(const vtkWebXRRenderWindowInteractor&) = delete;

  vtkNew<vtkMatrix4x4> PoseToWorldMatrix; // used in calculations
#ifdef __EMSCRIPTEN__
  WebXRGamepad LastGamepadState[2]; // support 2 controllers (left and right)
#endif
};

VTK_ABI_NAMESPACE_END
#endif
