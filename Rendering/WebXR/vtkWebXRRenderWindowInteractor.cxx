// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkWebXRRenderWindowInteractor.h"

#include "vtkObjectFactory.h"
#include "vtkVRRenderWindow.h"
#include "vtkWebXR.h"
#include "vtkWebXRInteractorStyle.h"
#include "vtkWebXRUtilities.h"
#include "vtk_jsoncpp.h"
#include <vtksys/FStream.hxx>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkWebXRRenderWindowInteractor);

//------------------------------------------------------------------------------
vtkWebXRRenderWindowInteractor::vtkWebXRRenderWindowInteractor()
{
  vtkNew<vtkWebXRInteractorStyle> style;
  this->SetInteractorStyle(style);

  // Action manifest is bundled in the WASM binary
  // It is located in the root directory of the virtual filesystem
  this->SetActionManifestDirectory("/");
  this->SetActionManifestFileName("vtk_webxr_actions.json");
}

//------------------------------------------------------------------------------
vtkWebXRRenderWindowInteractor::~vtkWebXRRenderWindowInteractor() = default;

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::PrintSelf(ostream& os, vtkIndent indent)
{
  os << indent << "vtkWebXRRenderWindowInteractor"
     << "\n";
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::Initialize()
{
  if (this->Initialized)
  {
    return;
  }

  // Start with superclass initialization
  this->Superclass::Initialize();

  vtkVRRenderWindow* renWin = vtkVRRenderWindow::SafeDownCast(this->RenderWindow);

  // Make sure the render window is initialized
  renWin->Initialize();

  if (!renWin->GetVRInitialized())
  {
    this->Initialized = false;
    return;
  }

  this->AddAction("handpose", vtkCommand::Move3DEvent);
  this->AddAction(
    "complexgestureaction", [this](vtkEventData* ed) { this->HandleComplexGestureEvents(ed); });

  std::string fullpath = vtksys::SystemTools::CollapseFullPath(
    this->ActionManifestDirectory + this->ActionManifestFileName);

  if (!this->LoadActions(fullpath))
  {
    vtkErrorMacro(<< "Failed to load actions.");
    this->Initialized = false;
    return;
  }
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::StartEventLoop()
{
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop([]() {}, 0, vtkRenderWindowInteractor::InteractorManagesTheEventLoop);
#endif
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::DoOneEvent(
  vtkVRRenderWindow* renWin, vtkRenderer* vtkNotUsed(ren))
{
  if (this->Done)
  {
    return;
  }

  this->UpdateXRControllers();

  if (this->RecognizeGestures)
  {
    this->RecognizeComplexGesture(nullptr);
  }

  // Start a render
  this->InvokeEvent(vtkCommand::RenderEvent);
  renWin->Render();
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::TerminateApp()
{
  this->SetDone(true);
  this->StopXR();
#ifdef __EMSCRIPTEN__
  emscripten_cancel_main_loop();
#endif
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::ConvertWebXRPoseToWorldCoordinates(
  const WebXRRigidTransform& xrPose,
  double pos[3],  // Output world position
  double wxyz[4], // Output world orientation quaternion
  double ppos[3], // Output physical position
  double wdir[3]) // Output world view direction (-Z)
{
  vtkWebXRUtilities::SetMatrixFromWebXRMatrix(this->PoseToWorldMatrix, xrPose.matrix);
  this->ConvertPoseToWorldCoordinates(this->PoseToWorldMatrix, pos, wxyz, ppos, wdir);
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::UpdateXRControllers()
{
#ifdef __EMSCRIPTEN__
  // first retrieve all input info
  WebXRInputSource input_sources[2];
  int nb_inputs;
  webxr_get_input_sources(input_sources, 2, &nb_inputs);

  WebXRRigidTransform input_poses[2][2];
  WebXRGamepad input_gamepad[2];
  vtkSmartPointer<vtkEventDataDevice3D> eventData[2][2];

  double position[3], orientation[4], physicalPosition[2][2][3], direction[3];
  for (WebXRHandedness hand : { WEBXR_HANDEDNESS_LEFT, WEBXR_HANDEDNESS_RIGHT })
  {
    if (!webxr_get_input_gamepad(&input_sources[hand], &input_gamepad[hand]))
    {
      return;
    }

    for (WebXRInputPoseMode inputPoseMode : { WEBXR_INPUT_POSE_GRIP, WEBXR_INPUT_POSE_TARGET_RAY })
    {
      if (!webxr_get_input_pose(
            &input_sources[hand], &input_poses[hand][inputPoseMode], inputPoseMode))
      {
        return;
      }

      this->ConvertWebXRPoseToWorldCoordinates(input_poses[hand][inputPoseMode], position,
        orientation, physicalPosition[hand][inputPoseMode], direction);
      vtkEventDataDevice edd = vtkWebXRUtilities::GetDeviceFromWebXRHand(hand);

      auto edHand = vtkEventDataDevice3D::New();
      edHand->SetDevice(edd);
      edHand->SetWorldPosition(position);
      edHand->SetWorldOrientation(orientation);
      edHand->SetWorldDirection(direction);
      eventData[hand][inputPoseMode].TakeReference(edHand);
    }
  }

  // then loop for each action
  for (const auto& elem : this->ButtonToActionMap)
  {
    const WebXRHandedness hand = elem.first.hand;
    const WebXRGamepadButtonMapping button_id = elem.first.button;
    const WebXRInputPoseMode mode = elem.first.mode;
    const ActionData* action = elem.second;

    const WebXRGamepadButton& button = input_gamepad[hand].buttons[button_id];
    const WebXRGamepadButton& last_state = LastGamepadState[hand].buttons[button_id];

    const auto& edp = eventData[hand][mode];

    if (action->IsHandPose)
    {
      const auto& wpos = edp->GetWorldPosition();
      const auto& wori = edp->GetWorldOrientation();

      int pointerIndex = static_cast<int>(edp->GetDevice());
      this->SetPointerIndex(pointerIndex);
      this->SetPhysicalEventPosition(physicalPosition[hand][mode][0],
        physicalPosition[hand][mode][1], physicalPosition[hand][mode][2], pointerIndex);
      this->SetWorldEventPosition(wpos[0], wpos[1], wpos[2], pointerIndex);
      this->SetWorldEventOrientation(wori[0], wori[1], wori[2], wori[3], pointerIndex);

      // Update DeviceToPhysical matrices, this is a read-write access!
      vtkVRRenderWindow* renWin = vtkVRRenderWindow::SafeDownCast(this->GetRenderWindow());
      vtkMatrix4x4* devicePose = renWin->GetDeviceToPhysicalMatrixForDevice(edp->GetDevice());
      if (devicePose)
      {
        vtkWebXRUtilities::SetMatrixFromWebXRMatrix(devicePose, input_poses[hand][mode].matrix);
      }

      edp->SetAction(vtkEventDataAction::Unknown);
      edp->SetType(action->EventId);
      this->ApplyAction(*action, edp);
    }
    else
    {
      if (button.pressed != last_state.pressed)
      {
        edp->SetAction(button.pressed ? vtkEventDataAction::Press : vtkEventDataAction::Release);
        edp->SetType(action->EventId);
        this->ApplyAction(*action, edp);
      }
    }
  }

  LastGamepadState[WEBXR_HANDEDNESS_LEFT] = input_gamepad[WEBXR_HANDEDNESS_LEFT];
  LastGamepadState[WEBXR_HANDEDNESS_RIGHT] = input_gamepad[WEBXR_HANDEDNESS_RIGHT];
#endif
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::StartXR([[maybe_unused]] SessionMode mode,
  [[maybe_unused]] vtkTypeUInt32 requiredFeatures, [[maybe_unused]] vtkTypeUInt32 optionalFeatures)
{
#ifdef __EMSCRIPTEN__
  webxr_request_session(static_cast<WebXRSessionMode>(mode), requiredFeatures, optionalFeatures);
#endif
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::StopXR()
{
#ifdef __EMSCRIPTEN__
  webxr_request_exit();
#endif
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::AddAction(
  const std::string& path, const vtkCommand::EventIds& eid)
{
  if (this->ActionMap.count(path) == 0)
  {
    this->ActionMap[path] = ActionData{};
  }

  auto& am = this->ActionMap[path];
  am.EventId = eid;
  am.UseFunction = false;
  am.IsHandPose = path == "handpose";
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::AddAction(
  const std::string& path, const std::function<void(vtkEventData*)>& func)
{
  if (this->ActionMap.count(path) == 0)
  {
    this->ActionMap[path] = ActionData{};
  }

  auto& am = this->ActionMap[path];
  am.UseFunction = true;
  am.Function = func;
  am.IsHandPose = path == "handpose";
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::AddAction(
  const std::string& path, const vtkCommand::EventIds& eid, bool vtkNotUsed(isAnalog))
{
  this->AddAction(path, eid);
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::AddAction(
  const std::string& path, bool vtkNotUsed(isAnalog), const std::function<void(vtkEventData*)>& eid)
{
  this->AddAction(path, eid);
}

//------------------------------------------------------------------------------
bool vtkWebXRRenderWindowInteractor::LoadActions(const std::string& actionFilename)
{
  vtkDebugMacro(<< "LoadActions from file : " << actionFilename);
  Json::Value root;

  // Open the file
  vtksys::ifstream file;
  file.open(actionFilename.c_str());
  if (!file.is_open())
  {
    vtkErrorMacro(<< "Unable to open WebXR action file : " << actionFilename);
    return false;
  }

  Json::CharReaderBuilder builder;

  std::string formattedErrors;
  // parse the entire data into the Json::Value root
  if (!Json::parseFromStream(builder, file, &root, &formattedErrors))
  {
    // Report failures and their locations in the document
    vtkErrorMacro(<< "Failed to parse action file with errors :" << endl << formattedErrors);
    return false;
  }

  // Create actions
  Json::Value actions = root["actions"];
  if (actions.isNull())
  {
    vtkErrorMacro(<< "Parse webxr_actions: Missing actions node");
    return false;
  }

  for (Json::Value::ArrayIndex i = 0; i < actions.size(); ++i)
  {
    // Create one action per json value
    const Json::Value& action = actions[i];

    std::string name = action["name"].asString();
    std::string hand = action["hand"].asString();

    std::string button = action["button"].asString();

    WebXRHandedness xrHand = WEBXR_HANDEDNESS_NONE;
    if (hand == "left")
    {
      xrHand = WEBXR_HANDEDNESS_LEFT;
    }
    else if (hand == "right")
    {
      xrHand = WEBXR_HANDEDNESS_RIGHT;
    }

    WebXRGamepadButtonMapping xrButton = WEBXR_INTERNAL_HANDPOSE;
    static const std::map<std::string, WebXRGamepadButtonMapping> jsonToXrButton = {
      { "trigger", WEBXR_BUTTON_PRIMARY_TRIGGER },
      { "grip", WEBXR_BUTTON_PRIMARY_SQUEEZE },
      { "joystick", WEBXR_BUTTON_PRIMARY_THUMBSTICK },
      { "trackpad", WEBXR_BUTTON_PRIMARY_TOUCHPAD },
      { "button1", WEBXR_BUTTON_1 },
      { "button2", WEBXR_BUTTON_2 },
      { "button3", WEBXR_BUTTON_3 },
      { "button4", WEBXR_BUTTON_4 },
    };

    WebXRInputPoseMode xrInputMode;
    // default to "aim"
    xrInputMode = WEBXR_INPUT_POSE_TARGET_RAY;
    if (name == "handpose")
    {
      std::string mode = action["mode"].asString();
      if (mode == "grip")
      {
        xrInputMode = WEBXR_INPUT_POSE_GRIP;
      }
    }
    else
    {
      auto it = jsonToXrButton.find(button);
      if (it == jsonToXrButton.end())
      {
        vtkErrorMacro(<< "Parse webxr_actions: Incorrect button type (" << button << ")");
        return false;
      }
      xrButton = it->second;
    }

    // Check if the action is used by the interactor style
    // or ourself. If that's the case, create it
    // Else do nothing
    {
      auto it = this->ActionMap.find(name);
      if (it == this->ActionMap.end())
      {
        vtkWarningMacro(
          << "An action with name " << name
          << " is available but the interactor style or the interactor does not use it.");
        continue;
      }

      vtkDebugMacro(<< "Creating an action of name=" << name);

      this->ButtonToActionMap[{ xrHand, xrButton, xrInputMode }] = &it->second;
    }
  }

  return true;
}

//------------------------------------------------------------------------------
const vtkWebXRRenderWindowInteractor::ActionData* vtkWebXRRenderWindowInteractor::GetActionData(
  const std::string& actionName)
{
  // Check if action data exists
  if (this->ActionMap.count(actionName) == 0)
  {
    vtkWarningMacro(<< "vtkWebXRRenderWindowInteractor: Attempt to get an action data with name "
                    << actionName << " that does not exist in the map.");
    return nullptr;
  }
  return &this->ActionMap[actionName];
}

//------------------------------------------------------------------------------
const vtkWebXRRenderWindowInteractor::ActionData* vtkWebXRRenderWindowInteractor::GetActionData(
  WebXRHandedness hand, WebXRGamepadButtonMapping button, WebXRInputPoseMode mode)
{
  auto it = ButtonToActionMap.find({ hand, button, mode });
  if (it != ButtonToActionMap.end())
  {
    return &*it->second;
  }
  else
  {
    return nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkWebXRRenderWindowInteractor::ApplyAction(
  const ActionData& actionData, vtkEventDataDevice3D* ed)
{
  if (actionData.UseFunction)
  {
    actionData.Function(ed);
  }
  else
  {
    this->InvokeEvent(actionData.EventId, ed);
  }
}

VTK_ABI_NAMESPACE_END
