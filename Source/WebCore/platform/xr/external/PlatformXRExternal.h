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

#pragma once

#if ENABLE(WEBXR) && USE(EXTERNALXR)

#include "GLContextEGL.h"
#include "GraphicsContextGL.h"
#include "PlatformXR.h"
#include "vr_external.h"

#include <wtf/HashMap.h>
#include <wtf/WorkQueue.h>
#include <jni.h>

namespace PlatformXR {

class XRHardwareBuffer;

class ExternalDevice final : public Device {
public:
    static Ref<ExternalDevice> create(JNIEnv*, Ref<WorkQueue>&&, CompletionHandler<void()>&&);

private:
    ExternalDevice(JNIEnv*, Ref<WorkQueue>&&);
    void initialize(CompletionHandler<void()>&& callback);

    // PlatformXR::Device
    WebCore::IntSize recommendedResolution(SessionMode) final;
    void initializeTrackingAndRendering(SessionMode) final;
    void shutDownTrackingAndRendering() final;
    void initializeReferenceSpace(PlatformXR::ReferenceSpaceType) final;
    bool supportsSessionShutdownNotification() const final { return true; }
    void requestFrame(RequestFrameCallback&&) final;
    void submitFrame(Vector<Device::Layer>&&) final;
    Vector<ViewData> views(SessionMode) const final;
    std::optional<LayerHandle> createLayerProjection(uint32_t width, uint32_t height, bool alpha) final;
    void deleteLayer(LayerHandle) final;

    // Custom methods
    void pushState(bool notifyCond = false);
    void pullState(const std::function<bool()>& waitCondition = { });

    JNIEnv* m_env { nullptr };
    WorkQueue& m_queue;
    std::unique_ptr<WebCore::GLContextEGL> m_egl;
    RefPtr<WebCore::GraphicsContextGL> m_gl;
    HashMap<LayerHandle, std::unique_ptr<XRHardwareBuffer>> m_layers;
    VRBrowserState m_browserState;
    VRSystemState m_systemState;
    VRExternalShmem* m_shmem { nullptr };
    bool m_enumerationCompleted { false };
    uint64_t m_frameId { 0 };
    LayerHandle m_layerIndex { 0 };
};

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
