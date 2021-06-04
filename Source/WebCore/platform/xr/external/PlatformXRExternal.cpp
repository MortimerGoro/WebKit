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

#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/threads/BinarySemaphore.h>

#define XR_LOGV(...) //__android_log_print(ANDROID_LOG_VERBOSE, "PlatformXR::ExternalDevice", __VA_ARGS__)
#define XR_LOGE(...) //__android_log_print(ANDROID_LOG_ERROR, "PlatformXR::ExternalDevice", __VA_ARGS__)

using namespace WebCore;

namespace PlatformXR {

static Device::FrameData::Pose toPose(const VRPose& p)
{
    Device::FrameData::Pose pose;
    pose.position = { p.position[0], p.position[1], p.position[2] };
    pose.orientation = { p.orientation[0], p.orientation[1], p.orientation[2], p.orientation[3] };
        
    return pose;
}

static Device::FrameData::Pose toPose(const float (&transform)[16])
{
    // TODO
    return { };
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

Ref<ExternalDevice> ExternalDevice::create(Ref<WorkQueue>&& queue, CompletionHandler<void()>&& callback)
{
    auto device = adoptRef(*new ExternalDevice(WTFMove(queue)));
    device->initialize(WTFMove(callback));
    return device;
}

ExternalDevice::ExternalDevice(Ref<WorkQueue>&& queue)
    : m_queue(WTFMove(queue))
{
}

void ExternalDevice::initialize(CompletionHandler<void()>&& callback)
{
    ASSERT(isMainThread());
    m_queue.dispatch([this, protectedThis = makeRef(*this), callback = WTFMove(callback)]() mutable {
        // Wait until the external shmem has valid data.
        pullState([this](){
            return m_enumerationCompleted;
        });

        callOnMainThread(WTFMove(callback));
    });
}

WebCore::IntSize ExternalDevice::recommendedResolution(SessionMode mode)
{
    if (!m_shmem)
        return { 0, 0 };

    int multiplier = mode == SessionMode::ImmersiveVr ? 2 : 1;

    return { multiplier * m_systemState.displayState.eyeResolution.width, m_systemState.displayState.eyeResolution.height };
}

void ExternalDevice::initializeTrackingAndRendering(SessionMode mode)
{
    m_queue.dispatch([this, protectedThis = makeRef(*this), mode]() {
        m_egl = GLContextEGL::createSharingContext(PlatformDisplay::sharedDisplay());
        if (!m_egl) {
            XR_LOGE("Failed to create EGL context");
            return;
        }

        auto& context = static_cast<GLContext&>(*m_egl);
        context.makeContextCurrent();

        GraphicsContextGLAttributes attributes;
        attributes.depth = false;
        attributes.stencil = false;
        attributes.antialias = false;

        m_gl = GraphicsContextGL::create(attributes, nullptr);
        if (!m_gl) {
            XR_LOGE("Failed to create a valid GraphicsContextGL");
            return;
        }

        XR_LOGV("Start presenting");
        m_browserState.presentationActive = true;
        m_browserState.layerState[0].type = VRLayerType::LayerType_Stereo_Immersive;
        pushState();

        m_frameId = 0;
    });
}

void ExternalDevice::shutDownTrackingAndRendering()
{
    m_queue.dispatch([this, protectedThis = makeRef(*this)]() {
        m_browserState.presentationActive = false;
        memset(m_browserState.layerState, 0, sizeof(VRLayerState) * PlatformXR::kVRLayerMaxCount);
        pushState(true);
    
        // deallocate graphic resources
        m_gl = nullptr;
        m_egl.reset();
    });
}

void ExternalDevice::initializeReferenceSpace(PlatformXR::ReferenceSpaceType)
{
}

void ExternalDevice::requestFrame(RequestFrameCallback&& callback)
{
    m_queue.dispatch([this, protectedThis = makeRef(*this), callback = WTFMove(callback)]() mutable {
        XR_LOGV("Request frame. Wait for frame > %d", m_frameId);
        pullState([this]() {
            return (m_systemState.sensorState.inputFrameID > m_frameId) || m_systemState.displayState.suppressFrames ||
                !m_systemState.displayState.isConnected;
        });
        m_frameId = m_systemState.sensorState.inputFrameID;
        const auto& display = m_systemState.displayState;

        Device::FrameData frameData;
        frameData.shouldRender = display.isConnected && !display.suppressFrames;

        XR_LOGV("Got frame %d. ShouldRender: %d", m_frameId, (int) frameData.shouldRender);

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
            FrameData::View leftView;
            leftView.projection = toProjection(display.eyeFOV[0]);
            leftView.offset = toPose(display.eyeTranslation[0]);

            FrameData::View rightView;
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

                FrameData::InputSource source;
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
                    FrameData::InputSourceButton button;
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

        callOnMainThread([frameData = WTFMove(frameData), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(frameData));
        });
    });
}

void ExternalDevice::submitFrame(Vector<Device::Layer>&& layers)
{
    m_queue.dispatch([this, protectedThis = makeRef(*this), layers = WTFMove(layers)]() mutable {
        XR_LOGV("Submit frame: %d", m_frameId);
        int index = 0;
        for (auto& layer : layers) {
            if (index >= PlatformXR::kVRLayerMaxCount)
                break;

            auto it = m_layers.find(layer.handle);
            if (it == m_layers.end()) {
                XR_LOGE(XR, "Didn't find a Layer with %d handle", layer.handle);
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

Vector<Device::ViewData> ExternalDevice::views(SessionMode mode) const
{
    Vector<Device::ViewData> views;
    if (mode == SessionMode::ImmersiveVr) {
        views.append({ .active = true, .eye = Eye::Left });
        views.append({ .active = true, .eye = Eye::Right });
    } else
        views.append({ .active = true, .eye = Eye::None });
    return views;
}

std::optional<LayerHandle> ExternalDevice::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    std::optional<LayerHandle> handle;

    BinarySemaphore semaphore;
    m_queue.dispatch([this, protectedThis = makeRef(*this), width, height, alpha, &handle, &semaphore]() mutable {
        auto signalOnExit = makeScopeExit([&semaphore]() {
            semaphore.signal();
        });

        if (!m_gl || !m_egl)
            return;

        if (auto buffer = XRHardwareBuffer::create(*m_egl, *m_gl, width, height)) {
            LayerHandle newHandle = ++m_layerIndex;
            m_layers.add(*newHandle, WTFMove(buffer));
            handle = newHandle;
        }
    });
    semaphore.wait();

    XR_LOGV("createLayerProjection: %d", handle.value_or(0));

    return handle;
}

void ExternalDevice::deleteLayer(LayerHandle handle)
{
    m_layers.remove(handle);
}

void ExternalDevice::pushState(bool notifyCond)
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

void ExternalDevice::pullState(const std::function<bool()>& waitCondition)
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

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)