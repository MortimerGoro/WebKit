/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <pthread.h>
#include <cstdint>
#include <type_traits>
#include <android/hardware_buffer.h>

namespace PlatformXR {

static const int32_t kVRExternalVersion = 18;

// We assign VR presentations to groups with a bitmask.
// Currently, we will only display either content or chrome.
// Later, we will have more groups to support VR home spaces and
// multitasking environments.
// These values are not exposed to regular content and only affect
// chrome-only API's.  They may be changed at any time.
static const uint32_t kVRGroupNone = 0;
static const uint32_t kVRGroupContent = 1 << 0;
static const uint32_t kVRGroupChrome = 1 << 1;
static const uint32_t kVRGroupAll = 0xffffffff;

static const int kVRDisplayNameMaxLen = 256;
static const int kVRControllerNameMaxLen = 256;
static const int kVRControllerMaxCount = 16;
static const int kVRControllerMaxButtons = 64;
static const int kVRControllerMaxAxis = 16;
static const int kVRLayerMaxCount = 8;
static const int kVRHapticsMaxCount = 32;

typedef AHardwareBuffer* VRLayerTextureHandle;

struct Point3D_POD {
  float x;
  float y;
  float z;
};

struct IntSize_POD {
  int32_t width;
  int32_t height;
};

struct FloatSize_POD {
  float width;
  float height;
};


enum class ControllerHand : uint8_t { _empty, Left, Right, EndGuard_ };

enum class ControllerCapabilityFlags : uint16_t {
  Cap_None = 0,
  /**
   * Cap_Position is set if the Gamepad is capable of tracking its position.
   */
  Cap_Position = 1 << 1,
  /**
   * Cap_Orientation is set if the Gamepad is capable of tracking its
   * orientation.
   */
  Cap_Orientation = 1 << 2,
  /**
   * Cap_AngularAcceleration is set if the Gamepad is capable of tracking its
   * angular acceleration.
   */
  Cap_AngularAcceleration = 1 << 3,
  /**
   * Cap_LinearAcceleration is set if the Gamepad is capable of tracking its
   * linear acceleration.
   */
  Cap_LinearAcceleration = 1 << 4,
  /**
   * Cap_TargetRaySpacePosition is set if the Gamepad has a grip space position.
   */
  Cap_GripSpacePosition = 1 << 5,
  /**
   * Cap_PositionEmulated is set if the XRInputSoruce is capable of setting a
   * emulated position (e.g. neck model) even if still doesn't support 6DOF
   * tracking.
   */
  Cap_PositionEmulated = 1 << 6,
  /**
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 7) - 1
};

enum class TargetRayMode : uint8_t { Gaze, TrackedPointer, Screen };

enum class GamepadMappingType : uint8_t { _empty, Standard, XRStandard };

enum class VRDisplayBlendMode : uint8_t { Opaque, Additive, AlphaBlend };

enum class VRDisplayCapabilityFlags : uint16_t {
  Cap_None = 0,
  /**
   * Cap_Position is set if the VRDisplay is capable of tracking its position.
   */
  Cap_Position = 1 << 1,
  /**
   * Cap_Orientation is set if the VRDisplay is capable of tracking its
   * orientation.
   */
  Cap_Orientation = 1 << 2,
  /**
   * Cap_Present is set if the VRDisplay is capable of presenting content to an
   * HMD or similar device.  Can be used to indicate "magic window" devices that
   * are capable of 6DoF tracking but for which requestPresent is not
   * meaningful. If false then calls to requestPresent should always fail, and
   * getEyeParameters should return null.
   */
  Cap_Present = 1 << 3,
  /**
   * Cap_External is set if the VRDisplay is separate from the device's
   * primary display. If presenting VR content will obscure
   * other content on the device, this should be un-set. When
   * un-set, the application should not attempt to mirror VR content
   * or update non-VR UI because that content will not be visible.
   */
  Cap_External = 1 << 4,
  /**
   * Cap_AngularAcceleration is set if the VRDisplay is capable of tracking its
   * angular acceleration.
   */
  Cap_AngularAcceleration = 1 << 5,
  /**
   * Cap_LinearAcceleration is set if the VRDisplay is capable of tracking its
   * linear acceleration.
   */
  Cap_LinearAcceleration = 1 << 6,
  /**
   * Cap_StageParameters is set if the VRDisplay is capable of room scale VR
   * and can report the StageParameters to describe the space.
   */
  Cap_StageParameters = 1 << 7,
  /**
   * Cap_MountDetection is set if the VRDisplay is capable of sensing when the
   * user is wearing the device.
   */
  Cap_MountDetection = 1 << 8,
  /**
   * Cap_PositionEmulated is set if the VRDisplay is capable of setting a
   * emulated position (e.g. neck model) even if still doesn't support 6DOF
   * tracking.
   */
  Cap_PositionEmulated = 1 << 9,
  /**
   * Cap_Inline is set if the device can be used for WebXR inline sessions
   * where the content is displayed within an elemen<pthread.h>t on the page.
   */
  Cap_Inline = 1 << 10,
  /**
   * Cap_ImmersiveVR is set if the device can give exclusive access to the
   * XR device display and that content is not intended to be integrated
   * with the user's environment
   */
  Cap_ImmersiveVR = 1 << 11,
  /**
   * Cap_ImmersiveAR is set if the device can give exclusive access to the
   * XR device display and that content is intended to be integrated with
   * the user's environment.
   */
  Cap_ImmersiveAR = 1 << 12,
  /**
   * Cap_UseDepthValues is set if the device will use the depth values of the
   * submitted frames if provided.  How the depth values are used is determined
   * by the VR runtime.  Often the depth is used for occlusion of system UI
   * or to enable more effective asynchronous reprojection of frames.
   */
  Cap_UseDepthValues = 1 << 13,
  /**
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 14) - 1
};


struct VRPose {
  float orientation[4];
  float position[3];
  float angularVelocity[3];
  float angularAcceleration[3];
  float linearVelocity[3];
  float linearAcceleration[3];
};

struct VRHMDSensorState {
  uint64_t inputFrameID;
  double timestamp;
  VRDisplayCapabilityFlags flags;

  // These members will only change with inputFrameID:
  VRPose pose;
  float leftViewMatrix[16];
  float rightViewMatrix[16];
};

struct VRFieldOfView {
  double upDegrees;
  double rightDegrees;
  double downDegrees;
  double leftDegrees;
};

struct VRDisplayState {
  enum Eye { Eye_Left, Eye_Right, NumEyes };

  // When true, indicates that the VR service has shut down
  bool shutdown;
  // Minimum number of milliseconds to wait before attempting
  // to start the VR service again
  uint32_t minRestartInterval;
  char displayName[kVRDisplayNameMaxLen];
  // eight byte character code identifier
  // LSB first, so "ABCDEFGH" -> ('H'<<56) + ('G'<<48) + ('F'<<40) +
  //                             ('E'<<32) + ('D'<<24) + ('C'<<16) +
  //                             ('B'<<8) + 'A').
  uint64_t eightCC;
  VRDisplayCapabilityFlags capabilityFlags;
  VRDisplayBlendMode blendMode;
  VRFieldOfView eyeFOV[VRDisplayState::NumEyes];
  Point3D_POD eyeTranslation[VRDisplayState::NumEyes];
  IntSize_POD eyeResolution;
  float nativeFramebufferScaleFactor;
  bool suppressFrames;
  bool isConnected;
  bool isMounted;
  FloatSize_POD stageSize;
  // We can't use a Matrix4x4 here unless we ensure it's a POD type
  float sittingToStandingTransform[16];
  uint64_t lastSubmittedFrameId;
  bool lastSubmittedFrameSuccessful;
  uint32_t presentingGeneration;
  // Telemetry
  bool reportsDroppedFrames;
  uint64_t droppedFrameCount;
};

struct VRControllerState {
  bool connected;
  char controllerName[kVRControllerNameMaxLen];
  char interactionProfile[kVRControllerNameMaxLen];
  ControllerHand hand;
  
  // https://immersive-web.github.io/webxr/#enumdef-xrtargetraymode
  TargetRayMode targetRayMode;

  // https://immersive-web.github.io/webxr-gamepads-module/#enumdef-gamepadmappingtype
  GamepadMappingType mappingType;

  // Start frame ID of the most recent primary select
  // action, or 0 if the select action has never occurred.
  uint64_t selectActionStartFrameId;
  // End frame Id of the most recent primary select
  // action, or 0 if action never occurred.
  // If selectActionStopFrameId is less than
  // selectActionStartFrameId, then the select
  // action has not ended yet.
  uint64_t selectActionStopFrameId;

  // start frame Id of the most recent primary squeeze
  // action, or 0 if the squeeze action has never occurred.
  uint64_t squeezeActionStartFrameId;
  // End frame Id of the most recent primary squeeze
  // action, or 0 if action never occurred.
  // If squeezeActionStopFrameId is less than
  // squeezeActionStartFrameId, then the squeeze
  // action has not ended yet.
  uint64_t squeezeActionStopFrameId;

  uint32_t numButtons;
  uint32_t numAxes;
  uint32_t numHaptics;
  // The current button pressed bit of button mask.
  uint64_t buttonPressed;
  // The current button touched bit of button mask.
  uint64_t buttonTouched;
  float triggerValue[kVRControllerMaxButtons];
  float axisValue[kVRControllerMaxAxis];

  ControllerCapabilityFlags flags;

  // When Cap_Position is set in flags, pose corresponds
  // to the controllers' pose in grip space:
  // https://immersive-web.github.io/webxr/#dom-xrinputsource-gripspace
  VRPose pose;

  // When Cap_TargetRaySpacePosition is set in flags, targetRayPose corresponds
  // to the controllers' pose in target ray space:
  // https://immersive-web.github.io/webxr/#dom-xrinputsource-targetrayspace
  VRPose targetRayPose;

  bool isPositionValid;
  bool isOrientationValid;
};

struct VRLayerEyeRect {
  float x;
  float y;
  float width;
  float height;
};

enum class VRLayerType : uint16_t {
  LayerType_None = 0,
  LayerType_2D_Content = 1,
  LayerType_Stereo_Immersive = 2
};

enum class VRLayerTextureType : uint16_t {
  LayerTextureType_None = 0,
  LayerTextureType_D3D10SurfaceDescriptor = 1,
  LayerTextureType_MacIOSurface = 2,
  LayerTextureType_GeckoSurfaceTexture = 3
};

struct VRLayer_2D_Content {
  VRLayerTextureHandle textureHandle;
  VRLayerTextureType textureType;
  uint64_t frameId;
};

struct VRLayer_Stereo_Immersive {
  VRLayerTextureHandle textureHandle;
  VRLayerTextureType textureType;
  uint64_t frameId;
  uint64_t inputFrameId;
  VRLayerEyeRect leftEyeRect;
  VRLayerEyeRect rightEyeRect;
  IntSize_POD textureSize;
};

struct VRLayerState {
  VRLayerType type;
  union {
    VRLayer_2D_Content layer_2d_content;
    VRLayer_Stereo_Immersive layer_stereo_immersive;
  };
};

struct VRHapticState {
  // Reference frame for timing.
  // When 0, this does not represent an active haptic pulse.
  uint64_t inputFrameID;
  // Index within VRSystemState.controllerState identifying the controller
  // to emit the haptic pulse
  uint32_t controllerIndex;
  // 0-based index indicating which haptic actuator within the controller
  uint32_t hapticIndex;
  // Start time of the haptic feedback pulse, relative to the start of
  // inputFrameID, in seconds
  float pulseStart;
  // Duration of the haptic feedback pulse, in seconds
  float pulseDuration;
  // Intensity of the haptic feedback pulse, from 0.0f to 1.0f
  float pulseIntensity;
};

struct VRBrowserState {
  bool shutdown;
  /**
   * In order to support WebXR's navigator.xr.IsSessionSupported call without
   * displaying any permission dialogue, it is necessary to have a safe way to
   * detect the capability of running a VR or AR session without activating XR
   * runtimes or powering on hardware.
   *
   * API's such as OpenVR make no guarantee that hardware and software won't be
   * left activated after enumerating devices, so each backend in gfx/vr/service
   * must allow for more granular detection of capabilities.
   *
   * When detectRuntimesOnly is true, the initialization exits early after
   * reporting the presence of XR runtime software.
   *
   * The result of the runtime detection is reported with the Cap_ImmersiveVR
   * and Cap_ImmersiveAR bits in VRDisplayState.flags.
   */
  bool detectRuntimesOnly;
  bool presentationActive;
  bool navigationTransitionActive;
  VRLayerState layerState[kVRLayerMaxCount];
  VRHapticState hapticState[kVRHapticsMaxCount];
};

struct VRSystemState {
  bool enumerationCompleted;
  VRDisplayState displayState;
  VRHMDSensorState sensorState;
  VRControllerState controllerState[kVRControllerMaxCount];
};

struct VRExternalShmem {
  int32_t version;
  int32_t size;
  pthread_mutex_t systemMutex;
  pthread_mutex_t geckoMutex;
  pthread_cond_t systemCond;
  pthread_cond_t geckoCond;
  VRBrowserState geckoState;
  VRSystemState systemState;
};

// As we are memcpy'ing VRExternalShmem and its members around, it must be a POD
// type
static_assert(std::is_pod<VRExternalShmem>::value,
              "VRExternalShmem must be a POD type.");

}  // namespace PlatformXR
