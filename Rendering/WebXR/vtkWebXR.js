// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Vhite Rabbit
// SPDX-FileCopyrightText: Copyright (c) Jonathan Hale
// SPDX-License-Identifier: BSD-3-Clause AND MIT

// File adapted from: https://github.com/WonderlandEngine/emscripten-webxr/blob/1bc0b7b66922d81f0bd0704f4b6e4a571b74eab2/library_webxr.js

var LibraryWebXR = {

$WebXR : {
  refSpaces : {},
  _curRAF : null,

  _nativize_vec3 : function(offset, vec) {
    setValue(offset + 0, vec.x, 'double');
    setValue(offset + 8, vec.y, 'double');
    setValue(offset + 16, vec.z, 'double');

    return offset + 24;
  },

  _nativize_vec4 : function(offset, vec) {
    WebXR._nativize_vec3(offset, vec);
    setValue(offset + 24, vec.w, 'double');

    return offset + 32;
  },

  _nativize_matrix : function(offset, mat) {
    for (var i = 0; i < 16; ++i)
    {
      setValue(offset + i * 8, mat[i], 'double');
    }

    return offset + 16 * 8;
  },

  _nativize_rigid_transform : function(offset, t) {
    offset = WebXR._nativize_matrix(offset, t.matrix);
    offset = WebXR._nativize_vec3(offset, t.position);
    offset = WebXR._nativize_vec4(offset, t.orientation);

    return offset;
  },

  // Sets input source values to offset and returns pointer after struct
  _nativize_input_source : function(offset, inputSource, id) {
    var handedness = -1;
    if (inputSource.handedness == "left")
    {
      handedness = 0;
    }
    else if (inputSource.handedness == "right")
    {
      handedness = 1;
    }

    var targetRayMode = 0;
    if (inputSource.targetRayMode == "tracked-pointer")
    {
      targetRayMode = 1;
    }
    else if (inputSource.targetRayMode == "screen")
    {
      targetRayMode = 2;
    }

    setValue(offset, id, 'i32');
    offset += 4;
    setValue(offset, handedness, 'i32');
    offset += 4;
    setValue(offset, targetRayMode, 'i32');
    offset += 4;

    return offset;
  },

  _nativize_gamepad_button : function(offset, button) {
    setValue(offset, button.pressed, 'i32');
    offset += 4;
    setValue(offset, button.touched, 'i32');
    offset += 4;
    setValue(offset, button.value, 'float');
    offset += 4;
    return offset;
  },
},

webxr_init__deps : [ '$dynCall' ],
webxr_init : function(
  frameCallback, startSessionCallback, endSessionCallback, errorCallback, userData) {
  function onError(errorCode)
  {
    if (!errorCallback)
    {
      return;
    }
    dynCall('vpi', errorCallback, [ userData, errorCode ]);
  };

  function onSessionEnd(mode)
  {
    if (!endSessionCallback)
    {
      return;
    }
    mode = { 'inline' : 0, 'immersive-vr' : 1, 'immersive-ar' : 2 }[mode];
    dynCall('vpi', endSessionCallback, [ userData, mode ]);
  };

  function onSessionStart(mode)
  {
    if (!startSessionCallback)
    {
      return;
    }
    mode = { 'inline' : 0, 'immersive-vr' : 1, 'immersive-ar' : 2 }[mode];
    dynCall('vpi', startSessionCallback, [ userData, mode ]);
  };

  function onFrame(time, frame)
  {
    if (!frameCallback)
    {
      return;
    }
    // Request next frame
    const session = frame.session;

    const glLayer = session.renderState.baseLayer;
    // If framebuffer is non-null, compositor is enabled and we bind it.
    // If it's null, we need to avoid this call otherwise the canvas FBO is bound
    if (glLayer.framebuffer)
    {
      // Make sure that FRAMEBUFFER_BINDING returns a valid value.
      // For that we create an id in the emscripten object tables
      // and add the frambuffer
      const id = Module.webxr_fbo || GL.getNewId(GL.framebuffers);
      glLayer.framebuffer.name = id;
      GL.framebuffers[id] = glLayer.framebuffer;
      Module.webxr_fbo = id;
    }

    // Set and reset environment for webxr_get_input_pose calls
    Module['webxr_frame'] = frame;
    dynCall('vpi', frameCallback, [ userData, time ]);

    // RAF is set to null on session end to avoid rendering
    if (Module['webxr_session'] != null)
      WebXR._curRAF = session.requestAnimationFrame(onFrame);
  };

  function onSessionStarted(session, mode)
  {
    Module['webxr_session'] = session;

    // React to session ending
    session.addEventListener('end', function() {
      Module['webxr_session'].cancelAnimationFrame(WebXR._curRAF);
      WebXR._curRAF = null;
      Module['webxr_session'] = null;
      Module['webxr_frame'] = null;
      Module.webxr_fbo = null;
      onSessionEnd(mode);
    });

    // Ensure our context can handle WebXR rendering
    Module.ctx.makeXRCompatible().then(
      function() {
        // Create the base layer
        const layer = Module['webxr_baseLayer'] = new window.XRWebGLLayer(session, Module.ctx, {
          framebufferScaleFactor : Module['webxr_framebuffer_scale_factor'],
        });
        session.updateRenderState({ baseLayer : layer });

        // 'viewer' reference space is always available.
        session.requestReferenceSpace('viewer').then(refSpace => {
          WebXR.refSpaces['viewer'] = refSpace;

          WebXR.refSpace = 'viewer';

          // Give application a chance to react to session starting
          // e.g. finish current desktop frame.
          onSessionStart(mode);

          // Start rendering
          session.requestAnimationFrame(onFrame);
        });

        // Request and cache other available spaces, which may not be available
        for (const s of ['local', 'local-floor', 'bounded-floor', 'unbounded'])
        {
          session.requestReferenceSpace(s).then(
            refSpace => {
              // We prefer the reference space automatically in above order
              WebXR.refSpace = s;

              WebXR.refSpaces[s] = refSpace;
            },
            function() {
              //Leave refSpaces[s] unset.
            }
          );
        }
      },
      function() { onError(-3); });
  };

  if (navigator.xr)
  {
    Module['webxr_request_session_func'] = function(mode, requiredFeatures, optionalFeatures) {
      if (typeof (mode) !== 'string')
      {
        mode = ([ 'inline', 'immersive-vr', 'immersive-ar' ])[mode];
      }
      let toFeatureList = function(bitMask) {
        const f = [];
        const features = [ 'local', 'local-floor', 'bounded-floor', 'unbounded', 'hit-test' ];
        for (let i = 0; i < features.length; ++i)
        {
          if ((bitMask & (1 << i)) != 0)
          {
            f.push(features[i]);
          }
        }
        return f;
      };
      if (typeof(requiredFeatures) === 'number')
      {
        requiredFeatures = toFeatureList(requiredFeatures);
      }
      if (typeof(optionalFeatures) === 'number')
      {
        optionalFeatures = toFeatureList(optionalFeatures);
      }
      navigator.xr
        .requestSession(
          mode, { requiredFeatures : requiredFeatures, optionalFeatures : optionalFeatures })
        .then(function(s) { onSessionStarted(s, mode); })
        .catch(console.error);
    };
  }
  else
  {
    // Call error callback with "WebXR not supported"
    onError(-2);
  }
},

webxr_is_session_supported__deps : [ '$dynCall' ],
webxr_is_session_supported : function(mode, callback) {
  if (!navigator.xr)
  {
    // WebXR not supported at all
    dynCall('vii', callback, [ mode, 0 ]);
    return;
  }
  navigator.xr.isSessionSupported(([ 'inline', 'immersive-vr', 'immersive-ar' ])[mode])
    .then(function() { dynCall('vii', callback, [ mode, 1 ]); },
      function() { dynCall('vii', callback, [ mode, 0 ]); });
},

webxr_request_session : function(mode, requiredFeatures, optionalFeatures) {
  var s = Module['webxr_request_session_func'];
  if (s)
    s(mode, requiredFeatures, optionalFeatures);
},

webxr_request_exit : function() {
  var s = Module['webxr_session'];
  if (s)
    Module['webxr_session'].end();
},

webxr_get_viewer_pose : function(outPosePtr) {
  const frame = Module['webxr_frame'];
  if (!frame)
  {
    return false;
  }

  const pose = frame.getViewerPose(WebXR.refSpaces[WebXR.refSpace]);
  if (!pose)
  {
    return false;
  }

  WebXR._nativize_rigid_transform(Number(outPosePtr), pose.transform);
},

webxr_get_views : function(outViewsPtr, viewCountPtr, maxViews) {
  const frame = Module['webxr_frame'];
  if (!frame)
  {
    return false;
  }

  const pose = frame.getViewerPose(WebXR.refSpaces[WebXR.refSpace]);
  if (!pose)
  {
    return false;
  }

  const session = Module['webxr_session'];
  if (!session)
  {
    return false;
  }

  const glLayer = session.renderState.baseLayer;
  let i = 0;
  outViewsPtr = Number(outViewsPtr); // for wasm64 compat
  for (i = 0; i < pose.views.length && i < maxViews; ++i)
  {
    const view = pose.views[i];
    const viewport = glLayer.getViewport(view);
    outViewsPtr = WebXR._nativize_rigid_transform(outViewsPtr, view.transform);
    outViewsPtr = WebXR._nativize_matrix(outViewsPtr, view.projectionMatrix);
    setValue(outViewsPtr + 0, viewport.x, 'i32');
    setValue(outViewsPtr + 4, viewport.y, 'i32');
    setValue(outViewsPtr + 8, viewport.width, 'i32');
    setValue(outViewsPtr + 12, viewport.height, 'i32');
    outViewsPtr += 16;
  }
  setValue(Number(viewCountPtr), i, 'i32');

  return true;
},

webxr_get_framebuffer_size : function(width, height) {
  const layer = Module['webxr_baseLayer'];
  if (!layer)
  {
    return false;
  }

  setValue(Number(width), layer.framebufferWidth, 'i32');
  setValue(Number(height), layer.framebufferHeight, 'i32');
  return true;
},

webxr_get_framebuffer : function() {
  return Module.webxr_fbo || 0;
},

webxr_get_input_sources : function(outArrayPtr, max, outCountPtr) {
  let s = Module['webxr_session'];
  if (!s)
  {
    return false;
  }

  let i = 0;
  outArrayPtr = Number(outArrayPtr); // for wasm64 compat
  for (let inputSource of s.inputSources)
  {
    if (i >= max)
      break;
    outArrayPtr = WebXR._nativize_input_source(outArrayPtr, inputSource, i);
    ++i;
  }
  setValue(Number(outCountPtr), i, 'i32');
  return true;
},

webxr_get_input_pose : function(source, outPosePtr, space) {
  let f = Module['webxr_frame'];
  if (!f)
  {
    console.warn("Cannot call webxr_get_input_pose outside of frame callback");
    return false;
  }

  const id = getValue(Number(source), 'i32');
  const input = Module['webxr_session'].inputSources[id];

  const s = space == 0 ? input.gripSpace : input.targetRaySpace;
  if (!s)
  {
    return false;
  }

  const pose = f.getPose(s, WebXR.refSpaces[WebXR.refSpace]);
  if (!pose || Number.isNaN(pose.transform.matrix[0]))
  {
    return false;
  }

  WebXR._nativize_rigid_transform(Number(outPosePtr), pose.transform);

  return true;
},

webxr_get_input_gamepad : function(source, outGamepadPtr) {
  let f = Module['webxr_frame'];
  if (!f)
  {
    console.warn("Cannot call webxr_get_input_gamepad outside of frame callback");
    return false;
  }

  const id = getValue(Number(source), 'i32');
  const input = Module['webxr_session'].inputSources[id];
  if (!input || !input.gamepad)
  {
    return false;
  }

  outGamepadPtr = Number(outGamepadPtr); // for wasm64 compat

  // buttons
  let i = 0;
  for (let button of input.gamepad.buttons)
  {
    // we support at most 8 buttons
    if (i >= 8)
      break;
    outGamepadPtr = WebXR._nativize_gamepad_button(outGamepadPtr, button);
    ++i;
  }
  // skip remaining buttons (each button is 3*4 bytes)
  outGamepadPtr += (8 - i) * 4 * 3;
  setValue(outGamepadPtr, i, 'i32');
  outGamepadPtr += 4;

  // axes
  i = 0;
  for (let axe of input.gamepad.axes)
  {
    // we support at most 8 axes
    if (i >= 8)
      break;
    setValue(outGamepadPtr, axe, 'float');
    outGamepadPtr += 4;
    ++i;
  }
  // skip remaining axes (each axis is 4 bytes)
  outGamepadPtr += (8 - i) * 4;
  setValue(outGamepadPtr, i, 'i32');

  return true;
}

};

autoAddDeps(LibraryWebXR, '$WebXR');
mergeInto(LibraryManager.library, LibraryWebXR);
