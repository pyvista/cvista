// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Vhite Rabbit
// SPDX-FileCopyrightText: Copyright (c) Jonathan Hale
// SPDX-License-Identifier: BSD-3-Clause AND MIT

// File adapted from:
// https://github.com/WonderlandEngine/emscripten-webxr/blob/1bc0b7b66922d81f0bd0704f4b6e4a571b74eab2/webxr.h

#ifndef vtkWebXR_h
#define vtkWebXR_h

#include "vtkABINamespace.h"

/** @file
 * @brief Minimal WebXR Device API wrapper
 */

#define webxr_init VTK_ABI_NAMESPACE_MANGLE(webxr_init)
#define webxr_is_session_supported VTK_ABI_NAMESPACE_MANGLE(webxr_is_session_supported)
#define webxr_request_session VTK_ABI_NAMESPACE_MANGLE(webxr_request_session)
#define webxr_request_exit VTK_ABI_NAMESPACE_MANGLE(webxr_request_exit)
#define webxr_get_viewer_pose VTK_ABI_NAMESPACE_MANGLE(webxr_get_viewer_pose)
#define webxr_get_views VTK_ABI_NAMESPACE_MANGLE(webxr_get_views)
#define webxr_get_framebuffer_size VTK_ABI_NAMESPACE_MANGLE(webxr_get_framebuffer_size)
#define webxr_get_framebuffer VTK_ABI_NAMESPACE_MANGLE(webxr_get_framebuffer)
#define webxr_get_input_sources VTK_ABI_NAMESPACE_MANGLE(webxr_get_input_sources)
#define webxr_get_input_pose VTK_ABI_NAMESPACE_MANGLE(webxr_get_input_pose)
#define webxr_get_input_gamepad VTK_ABI_NAMESPACE_MANGLE(webxr_get_input_gamepad)

#ifdef __cplusplus
extern "C"
#endif
{

  /** Errors enum */
  enum WebXRError
  {
    WEBXR_ERR_API_UNSUPPORTED = -2,     /**< WebXR Device API not supported in this browser */
    WEBXR_ERR_GL_INCAPABLE = -3,        /**< GL context cannot render WebXR */
    WEBXR_ERR_SESSION_UNSUPPORTED = -4, /**< given session mode not supported */
  };

  /** WebXR handedness */
  enum WebXRHandedness
  {
    WEBXR_HANDEDNESS_NONE = -1,
    WEBXR_HANDEDNESS_LEFT = 0,
    WEBXR_HANDEDNESS_RIGHT = 1,
  };

  /** WebXR target ray mode */
  enum WebXRTargetRayMode
  {
    WEBXR_TARGET_RAY_MODE_GAZE = 0,
    WEBXR_TARGET_RAY_MODE_TRACKED_POINTER = 1,
    WEBXR_TARGET_RAY_MODE_SCREEN = 2,
  };

  /** WebXR 'XRSessionMode' enum*/
  enum WebXRSessionMode
  {
    WEBXR_SESSION_MODE_INLINE = 0,
    WEBXR_SESSION_MODE_IMMERSIVE_VR = 1,
    WEBXR_SESSION_MODE_IMMERSIVE_AR = 2,
  };

/** WebXR 'XRSessionFeatures'*/
#define WEBXR_SESSION_FEATURE_LOCAL 0x01
#define WEBXR_SESSION_FEATURE_LOCAL_FLOOR 0x02
#define WEBXR_SESSION_FEATURE_BOUNDED_FLOOR 0x04
#define WEBXR_SESSION_FEATURE_UNBOUNDED 0x08
#define WEBXR_SESSION_FEATURE_HIT_TEST 0x10

  /** WebXR 'XRSessionMode' enum*/
  enum WebXRInputPoseMode
  {
    WEBXR_INPUT_POSE_GRIP = 0,
    WEBXR_INPUT_POSE_TARGET_RAY = 1,
  };

  /** WebXR rigid transform */
  typedef struct WebXRRigidTransform
  {
    double matrix[16];
    double position[3];
    double orientation[4];
  } WebXRRigidTransform;

  /** WebXR view */
  typedef struct WebXRView
  {
    /* view pose */
    WebXRRigidTransform viewPose;
    /* projection matrix */
    double projectionMatrix[16];
    /* x, y, width, height of the eye viewport on target texture */
    int viewport[4];
  } WebXRView;

  typedef struct WebXRInputSource
  {
    int id;
    WebXRHandedness handedness;
    WebXRTargetRayMode targetRayMode;
  } WebXRInputSource;

  typedef struct WebXRGamepadButton
  {
    int pressed;
    int touched;
    float value;
  } WebXRGamepadButton;

  enum WebXRGamepadButtonMapping
  {
    WEBXR_BUTTON_PRIMARY_TRIGGER = 0,
    WEBXR_BUTTON_PRIMARY_SQUEEZE = 1,
    WEBXR_BUTTON_PRIMARY_TOUCHPAD = 2,
    WEBXR_BUTTON_PRIMARY_THUMBSTICK = 3,
    WEBXR_BUTTON_1 = 4,
    WEBXR_BUTTON_2 = 5,
    WEBXR_BUTTON_3 = 6,
    WEBXR_BUTTON_4 = 7,
    WEBXR_INTERNAL_HANDPOSE = 8,
  };

  enum WebXRGamepadAxisMapping
  {
    WEBXR_AXIS_PRIMARY_TOUCHPAD_X = 0,
    WEBXR_AXIS_PRIMARY_TOUCHPAD_Y = 1,
    WEBXR_AXIS_PRIMARY_THUMBSTICK_X = 2,
    WEBXR_AXIS_PRIMARY_THUMBSTICK_Y = 3,
    WEBXR_AXIS_1 = 4,
    WEBXR_AXIS_2 = 5,
    WEBXR_AXIS_3 = 6,
    WEBXR_AXIS_4 = 7,
  };

  /**
  WebXR Gamepad
  see https://www.w3.org/TR/2024/WD-webxr-gamepads-module-1-20240409/#xr-standard-heading
  */
  typedef struct WebXRGamepad
  {
    // Max 8 buttons per gamepad (4 standard + 4 custom)
    WebXRGamepadButton buttons[8];
    int buttonsCount;
    // Max 8 axes per gamepad (4 standard + 4 custom)
    float axes[8];
    int axesCount;
  } WebXRGamepad;

  /**
  Callback for errors

  @param userData User pointer passed to init_webxr()
  @param error Error code
  */
  typedef void (*webxr_error_callback_func)(void* userData, int error);

  /**
  Callback for frame rendering

  @param userData User pointer passed to init_webxr()
  @param time Current frame time
  */
  typedef void (*webxr_frame_callback_func)(void* userData, int time);

  /**
  Callback for VR session start

  @param userData User pointer passed to set_session_start_callback
  @param mode The session mode
  */
  typedef void (*webxr_session_callback_func)(void* userData, int mode);

  /**
  Callback for @ref webxr_is_session_supported

  @param mode The session mode that was requested
  @param supported Whether given mode is supported by this device
  */
  typedef void (*webxr_session_supported_callback_func)(int mode, int supported);

  /**
  Init WebXR rendering

  @param frameCallback Callback called every frame
  @param sessionStartCallback Callback called when session is started
  @param sessionEndCallback Callback called when session ended
  @param errorCallback Callback called every frame
  @param userData User data passed to the callbacks
  */
  extern void webxr_init(webxr_frame_callback_func frameCallback,
    webxr_session_callback_func sessionStartCallback,
    webxr_session_callback_func sessionEndCallback, webxr_error_callback_func errorCallback,
    void* userData);

  /*
  Test if session mode is supported

  @param mode Session mode to test
  @param supportedCallback Callback which will be called once the
          result has become available
  */
  extern void webxr_is_session_supported(
    WebXRSessionMode mode, webxr_session_supported_callback_func supportedCallback);
  /*
  Request session presentation start

  @param mode Session mode from @ref WebXRSessionMode.
  @param requiredFeatures Required session features from @ref WebXRSessionFeatures
  @param optionalFeatures Optional session features from @ref WebXRSessionFeatures

  Needs to be called from a [user activation
  event](https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation).
  */
  extern void webxr_request_session(
    WebXRSessionMode mode, unsigned int requiredFeatures, unsigned int optionalFeatures);

  /*
  Request that the webxr presentation exits VR mode
  */
  extern void webxr_request_exit();

  /**
  Get viewer (hmd) pose
  */
  extern bool webxr_get_viewer_pose(WebXRRigidTransform* pose);

  /**
  Get views
  */
  extern bool webxr_get_views(WebXRView* views, int* viewCount, int maxViews);

  /**
  Get framebuffer width and height
  */
  extern bool webxr_get_framebuffer_size(int* width, int* height);

  /**
  Get WebXR framebuffer ID
  */
  extern unsigned int webxr_get_framebuffer();

  /**

  WebXR Input

  */

  /**
  Get input sources.

  @param outArray @ref WebXRInputSource array to fill.
  @param max Size of outArray (in elements).
  @param outCount Will receive the number of input sources valid in outArray.
  @returns `false` if getting the input sources failed, `true` otherwise.
  */
  extern int webxr_get_input_sources(WebXRInputSource* outArray, int max, int* outCount);

  /**
  Get input pose. Can only be called during the frame callback.

  @param source The source to get the pose for.
  @param outPose Where to store the pose.
  @param mode Input pose mode, whether `WEBXR_INPUT_POSE_GRIP` or `WEBXR_INPUT_POSE_TARGET_RAY`
  @returns `false` if updating the pose failed, `true` otherwise.
  */
  extern int webxr_get_input_pose(WebXRInputSource* source, WebXRRigidTransform* outPose,
    WebXRInputPoseMode mode = WEBXR_INPUT_POSE_GRIP);

  /**
  Get input gamepad. Can only be called during the frame callback.

  @param source The source to get the gamepad for.
  @param outGamepad Where to store the gamepad.
  @returns `false` if updating the gamepad failed, `true` otherwise
  */
  extern int webxr_get_input_gamepad(WebXRInputSource* source, WebXRGamepad* outGamepad);
}

#endif
// VTK-HeaderTest-Exclude: vtkWebXR.h
