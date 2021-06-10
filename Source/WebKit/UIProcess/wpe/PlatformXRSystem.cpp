/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformXRSystem.h"

#if ENABLE(WEBXR) && USE(EXTERNALXR)

#include "WebCoreArgumentCoders.h"
#include "PlatformXRCoordinator.h"
#include "PlatformXRSystemProxyMessages.h"
#include "PlatformXRExternal.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

PlatformXRSystem::PlatformXRSystem(WebPageProxy& page)
    : m_page(page)
{
    m_page.process().addMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), m_page.webPageID(), *this);
}

PlatformXRSystem::~PlatformXRSystem()
{
    m_page.process().removeMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), m_page.webPageID());
}

void PlatformXRSystem::invalidate()
{
    if (xrCoordinator())
        xrCoordinator()->endSessionIfExists(m_page);
}

void PlatformXRSystem::enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&& completionHandler)
{
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler({ });
        return;
    }

    xrCoordinator->getPrimaryDeviceInfo([completionHandler = WTFMove(completionHandler)](std::optional<XRDeviceInfo> deviceInfo) mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), deviceInfo = WTFMove(deviceInfo)]() mutable {
            if (!deviceInfo) {
                completionHandler({ });
                return;
            }

            completionHandler({ deviceInfo.value() });
        });
    });
}

void PlatformXRSystem::initializeTrackingAndRendering()
{
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator)
        return;

    xrCoordinator->startSession(m_page, [weakThis = makeWeakPtr(*this)](XRDeviceIdentifier deviceIdentifier) {
        RunLoop::main().dispatch([weakThis, deviceIdentifier]() mutable {
            if (weakThis)
                weakThis->m_page.send(Messages::PlatformXRSystemProxy::SessionDidEnd(deviceIdentifier));
        });
    });
}

void PlatformXRSystem::shutDownTrackingAndRendering()
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->endSessionIfExists(m_page);
}

void PlatformXRSystem::requestFrame(CompletionHandler<void(PlatformXR::Device::FrameData&&)>&& completionHandler)
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->scheduleAnimationFrame(m_page, WTFMove(completionHandler));
}

void PlatformXRSystem::submitFrame(Vector<PlatformXR::Device::Layer>&& layers)
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->submitFrame(m_page, WTFMove(layers));
}

void PlatformXRSystem::createLayerProjection(uint32_t width, uint32_t height, bool alpha, Messages::PlatformXRSystem::CreateLayerProjection::DelayedReply&& reply)
{
    std::optional<int> handle;
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        handle = xrCoordinator->createLayerProjection(m_page, width, height, alpha);

    reply(handle);
}

}

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/PlatformXRSystemAdditions.mm>
#else
namespace WebKit {

PlatformXRCoordinator* PlatformXRSystem::xrCoordinator()
{
    if (!m_external) {
        m_external = PlatformXRExternal::create();
    }

    return m_external.get();
}

}
#endif

#endif // ENABLE(WEBXR)
