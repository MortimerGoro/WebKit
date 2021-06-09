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

#include "PlatformXRCoordinator.h"
#include "vr_external.h"

#include <wtf/HashMap.h>
#include <wtf/WorkQueue.h>
#include <jni.h>

namespace WebKit {

class XRHardwareBuffer;

class PlatformXRExternal final : private PlatformXRCoordinator {
public:
    static std::unique_ptr<PlatformXRExternal> create();

private:
    PlatformXRExternal(JNIEnv*, PlatformXR::VRExternalShmem*, Ref<WorkQueue>&&);

    void getPrimaryDeviceInfo(DeviceInfoCallback&&) override;
    void startSession(WebPageProxy&, OnSessionEndCallback&&) override;
    void endSessionIfExists(WebPageProxy&) override;
    void scheduleAnimationFrame(WebPageProxy&, PlatformXR::Device::RequestFrameCallback&&) override;
    void submitFrame(WebPageProxy&, Vector<PlatformXR::Device::Layer>&&) override;
    std::optional<PlatformXR::LayerHandle> createLayerProjection(WebPageProxy&, uint32_t width, uint32_t height, bool alpha) override;

    // Custom methods
    void pushState(bool notifyCond = false);
    void pullState(const std::function<bool()>& waitCondition = { });

    JNIEnv* m_env { nullptr };
    PlatformXR::VRExternalShmem* m_shmem { nullptr };
    WorkQueue& m_queue;
    XRDeviceIdentifier m_identifier;
    HashMap<PlatformXR::LayerHandle, std::unique_ptr<XRHardwareBuffer>> m_layers;
    PlatformXR::VRBrowserState m_browserState;
    PlatformXR::VRSystemState m_systemState;
    bool m_enumerationCompleted { false };
    uint64_t m_frameId { 0 };
    PlatformXR::LayerHandle m_layerIndex { 0 };
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
