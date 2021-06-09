/*
 * Copyright (C) 2021 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(WEBXR) && USE(EXTERNALXR)
#include "PlatformXRExternal.h"
#include "XRHardwareBuffer.h"
#include <WebCore/TransformationMatrix.h>

#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/Scope.h>
#include <wtf/threads/BinarySemaphore.h>

#include <android/log.h>
#define XR_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "PlatformXR::PlatformXRExternal", __VA_ARGS__)
#define XR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PlatformXR::PlatformXRExternal", __VA_ARGS__)

using namespace WebCore;
using namespace PlatformXR;

namespace WebKit {

static Device::FrameData::Pose toPose(const VRPose& p)
{
    Device::FrameData::Pose pose;
    pose.position = { p.position[0], p.position[1], p.position[2] };
    pose.orientation = { p.orientation[0], p.orientation[1], p.orientation[2], p.orientation[3] };
        
    return pose;
}

static Device::FrameData::Pose toPose(const float (&m)[16])
{
    Device::FrameData::Pose pose { };

    TransformationMatrix matrix(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9],
        m[10], m[11], m[12], m[13], m[14], m[15]);

    if (matrix.isIdentity()) {
        // TransformationMatrix::decompose returns a empty quaternion instead of unit quaternion for Identity.
        return pose;
    }

    TransformationMatrix::Decomposed4Type decomp = { };
    matrix.decompose4(decomp);

    
    pose.orientation = { static_cast<float>(-decomp.quaternionX), static_cast<float>(-decomp.quaternionY),
        static_cast<float>(-decomp.quaternionZ), static_cast<float>(decomp.quaternionW) };
    pose.position = { static_cast<float>(decomp.translateX), static_cast<float>(decomp.translateY), static_cast<float>(decomp.translateZ) };
    return pose;
}

static Device::FrameData::Pose toPose(const Point3D_POD& translation)
{
    Device::FrameData::Pose pose;
    pose.position = { translation.x, translation.y, translation.z };
    pose.orientation = { 0.0, 0.0, 0.0, 1.0};
    return pose;
}

static Device::FrameData::Projection toProjection(const VRFieldOfView& fov)
{
    auto convert = [](double degrees) {
        return static_cast<float>(std::fabs(deg2rad(degrees)));
    };

    return Device::FrameData::Fov { convert(fov.upDegrees), convert(fov.downDegrees), convert(fov.leftDegrees), convert(fov.rightDegrees) };
}

std::unique_ptr<PlatformXRExternal> PlatformXRExternal::create()
{
    return nullptr;
}

PlatformXRExternal::PlatformXRExternal(JNIEnv* env, PlatformXR::VRExternalShmem* shmem, Ref<WorkQueue>&& queue)
    : m_env(env)
    , m_shmem(shmem)
    , m_queue(WTFMove(queue))
    , m_identifier(XRDeviceIdentifier::generate())
{
}

void PlatformXRExternal::getPrimaryDeviceInfo(DeviceInfoCallback&& callback)
{
    ASSERT(isMainThread());
    m_queue.dispatch([this, callback = WTFMove(callback)]() mutable {
        // Wait until the external shmem has valid data.
        pullState([this](){
            return m_enumerationCompleted;
        });

        XRDeviceInfo info;
        info.identifier = m_identifier;
        info.supportsOrientationTracking = true;
        info.supportsStereoRendering = true;
        info.recommendedResolution = {
            2 * m_systemState.displayState.eyeResolution.width, m_systemState.displayState.eyeResolution.height
        };

        callback(info);
    });
}

void PlatformXRExternal::startSession(WebPageProxy&, OnSessionEndCallback&&)
{
    m_queue.dispatch([this]() {
        XR_LOGV("Start presenting");
        m_browserState.presentationActive = true;
        m_browserState.layerState[0].type = VRLayerType::LayerType_Stereo_Immersive;
        pushState();

        m_frameId = 0;
    });
}

void PlatformXRExternal::endSessionIfExists(WebPageProxy&)
{
    m_queue.dispatch([this]() {
        m_browserState.presentationActive = false;
        memset(m_browserState.layerState, 0, sizeof(VRLayerState) * PlatformXR::kVRLayerMaxCount);
        pushState(true);
    });
}

void PlatformXRExternal::scheduleAnimationFrame(WebPageProxy&, PlatformXR::Device::RequestFrameCallback&& callback)
{
    m_queue.dispatch([this, callback = WTFMove(callback)]() mutable {
        XR_LOGV("Request frame. Wait for frame > %lu", m_frameId);
        pullState([this]() {
            return (m_systemState.sensorState.inputFrameID > m_frameId) || m_systemState.displayState.suppressFrames ||
                !m_systemState.displayState.isConnected;
        });
        m_frameId = m_systemState.sensorState.inputFrameID;
        const auto& display = m_systemState.displayState;

        Device::FrameData frameData;
        frameData.shouldRender = display.isConnected && !display.suppressFrames;

        XR_LOGV("Got frame %lu. ShouldRender: %d", m_frameId, (int) frameData.shouldRender);

        if (frameData.shouldRender) {
            auto& sensor = m_systemState.sensorState;

            auto supportsFlag = [&sensor](VRDisplayCapabilityFlags flag) {
                return (static_cast<int>(sensor.flags) & static_cast<int>(flag)) != 0;
            };

            // Tracking status
            frameData.isTrackingValid = supportsFlag(VRDisplayCapabilityFlags::Cap_Orientation);
            bool supportsPosition = supportsFlag(VRDisplayCapabilityFlags::Cap_Position);;
            bool supportsEmulatedPosition = supportsFlag(VRDisplayCapabilityFlags::Cap_PositionEmulated);
            frameData.isPositionValid = supportsPosition || supportsEmulatedPosition;
            frameData.isPositionEmulated = supportsEmulatedPosition && !supportsPosition;

            // Layers
            for (auto& layer : m_layers) {
                auto layerData = layer.value->startFrame();
                frameData.layers.add(layer.key, layerData);
            }

            // Pose
            frameData.predictedDisplayTime = static_cast<long>(sensor.timestamp);
            frameData.origin = toPose(sensor.pose);

            // Views: Projection matrix and eye offset
            Device::FrameData::View leftView;
            leftView.projection = toProjection(display.eyeFOV[0]);
            leftView.offset = toPose(display.eyeTranslation[0]);

            Device::FrameData::View rightView;
            rightView.projection = toProjection(display.eyeFOV[1]);
            rightView.offset = toPose(display.eyeTranslation[1]);

            frameData.views = { leftView, rightView };

            // Stage parameters
            if (supportsFlag(VRDisplayCapabilityFlags::Cap_StageParameters)) {
                frameData.floorTransform = { toPose(display.sittingToStandingTransform) };

                const auto& stageSize = display.stageSize;
                if (stageSize.width > 0 && stageSize.height > 0) {
                    frameData.stageParameters.bounds = Vector<WebCore::FloatPoint> {
                        { stageSize.width * 0.5f, -stageSize.height * 0.5f },
                        { stageSize.width * 0.5f, stageSize.height * 0.5f },
                        { -stageSize.width * 0.5f, stageSize.height * 0.5f },
                        { -stageSize.width * 0.5f, -stageSize.height * 0.5f }
                    };
                }
            }

            // Input sources
            for (uint32_t i = 0; i < kVRControllerMaxCount; ++i) {
                const auto& controller = m_systemState.controllerState[i];
                if (!controller.connected)
                    continue;

                Device::FrameData::InputSource source;
                source.handeness = controller.hand == ControllerHand::Left ? XRHandedness::Left : XRHandedness::Right;
                source.handle = i;
                // TODO: Get from external
                source.profiles = {
                    "oculus-touch-v3", "oculus-touch-v2",
                    "oculus-touch", "generic-trigger-squeeze-thumbstick"
                };

                // Poses
                switch (controller.targetRayMode){
                    case TargetRayMode::Gaze:
                        source.targetRayMode = XRTargetRayMode::Gaze;
                        break;
                    case TargetRayMode::Screen:
                        source.targetRayMode = XRTargetRayMode::Screen;
                        break;
                    case TargetRayMode::TrackedPointer:
                        source.targetRayMode = XRTargetRayMode::TrackedPointer;
                        break;
                }

                auto supportsControllerFlag = [&controller](ControllerCapabilityFlags flag) {
                    return (static_cast<int>(controller.flags) & static_cast<int>(flag)) != 0;
                };
                bool controllerPositionEmulated = !supportsControllerFlag(ControllerCapabilityFlags::Cap_Position) 
                    && supportsControllerFlag(ControllerCapabilityFlags::Cap_PositionEmulated);

                source.pointerOrigin = { toPose(controller.targetRayPose), controllerPositionEmulated };
                source.gripOrigin = { toPose(controller.pose), controllerPositionEmulated };

                // Buttons
                for (uint32_t i = 0; i < controller.numButtons; ++i) {
                    Device::FrameData::InputSourceButton button;
                    button.pressed = (controller.buttonPressed & (1 << i)) != 0;
                    button.touched = (controller.buttonTouched & (1 << i)) != 0;
                    button.pressedValue = controller.triggerValue[i];
                    source.buttons.append(button);
                }
                // Axes
                for (uint32_t i = 0; i < controller.numAxes; ++i) {
                    source.axes.append(controller.axisValue[i]);
                }

                frameData.inputSources.append(WTFMove(source));
            }

        }

        callback(WTFMove(frameData));
    });
}

void PlatformXRExternal::submitFrame(WebPageProxy&, Vector<PlatformXR::Device::Layer>&& layers)
{
    m_queue.dispatch([this, layers = WTFMove(layers)]() mutable {
        XR_LOGV("Submit frame: %lu", m_frameId);
        int index = 0;
        for (auto& layer : layers) {
            if (index >= PlatformXR::kVRLayerMaxCount)
                break;

            auto it = m_layers.find(layer.handle);
            if (it == m_layers.end()) {
                XR_LOGE("Didn't find a Layer with %d handle", layer.handle);
                continue;
            }

            it->value->endFrame();
            
            VRLayer_Stereo_Immersive& externalLayer = m_browserState.layerState[index++].layer_stereo_immersive;
            externalLayer.frameId = m_frameId;

            for (auto& view: layer.views) {
                auto& externalRect = view.eye == Eye::Left ? externalLayer.leftEyeRect : externalLayer.rightEyeRect;
                externalRect.x =  view.viewport.x();
                externalRect.y =  view.viewport.y();
                externalRect.width =  view.viewport.width();
                externalRect.height =  view.viewport.height();
            }
        }
    });
}

std::optional<LayerHandle> PlatformXRExternal::createLayerProjection(WebPageProxy&, uint32_t width, uint32_t height, bool alpha)
{
    std::optional<LayerHandle> handle;

    BinarySemaphore semaphore;
    m_queue.dispatch([this, protectedThis = makeRef(*this), width, height, alpha, &handle, &semaphore]() mutable {
        auto signalOnExit = makeScopeExit([&semaphore]() {
            semaphore.signal();
        });

        if (auto buffer = XRHardwareBuffer::create(m_env, width, height, alpha)) {
            LayerHandle newHandle = ++m_layerIndex;
            m_layers.add(newHandle, WTFMove(buffer));
            handle = newHandle;
        }
    });
    semaphore.wait();

    XR_LOGV("createLayerProjection: %d", handle.value_or(0));

    return handle;
}

void PlatformXRExternal::deleteLayer(LayerHandle handle)
{
    m_queue.dispatch([this]() mutable {
        m_layers.remove(handle);
    });
}

void PlatformXRExternal::pushState(bool notifyCond)
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    if (!m_shmem)
        return;

    if (pthread_mutex_lock((pthread_mutex_t*)&(m_shmem->geckoMutex)) == 0) {
        memcpy((void*)&(m_shmem->geckoState), (void*)&m_browserState, sizeof(VRBrowserState));
        if (notifyCond)
            pthread_cond_signal((pthread_cond_t*)&(m_shmem->geckoCond));
        pthread_mutex_unlock((pthread_mutex_t*)&(m_shmem->geckoMutex));
    }
}

void PlatformXRExternal::pullState(const std::function<bool()>& waitCondition)
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    if (!m_shmem)
        return;

    bool done = false;
    while (!done) {
        if (pthread_mutex_lock((pthread_mutex_t*)&m_shmem->systemMutex) == 0) {
            while (true) {
                memcpy(&m_systemState, (void*)&m_shmem->systemState, sizeof(VRSystemState));
                if (!waitCondition || waitCondition()) {
                    done = true;
                    break;
                }
                // Block current thead using the condition variable until data
                // changes
                pthread_cond_wait((pthread_cond_t*)&m_shmem->systemCond, (pthread_mutex_t*)&m_shmem->systemMutex);
            } // while (true)
            pthread_mutex_unlock((pthread_mutex_t*)&(m_shmem->systemMutex));
        } else if (!waitCondition) {
            // pthread_mutex_lock failed and we are not waiting for a condition to
            // exit from PullState call.
            return;
        }
    } // while (!done) {
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)