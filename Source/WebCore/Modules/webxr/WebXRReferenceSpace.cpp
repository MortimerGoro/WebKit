/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebXRReferenceSpace.h"

#if ENABLE(WEBXR)

#include "Document.h"
#include "WebXRFrame.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRReferenceSpace);

Ref<WebXRReferenceSpace> WebXRReferenceSpace::create(Document& document, Ref<WebXRSession>&& session, XRReferenceSpaceType type)
{
	// https://immersive-web.github.io/webxr/#xrspace-native-origin
    // The transform from the effective space to the native origin's space is
    // defined by an origin offset, which is an XRRigidTransform initially set
    // to an identity transform.
	Ref<WebXRRigidTransform> offset = WebXRRigidTransform::create();

    return adoptRef(*new WebXRReferenceSpace(document, WTFMove(session), WTFMove(offset), type));
}

Ref<WebXRReferenceSpace> WebXRReferenceSpace::create(Document& document, Ref<WebXRSession>&& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
{
    return adoptRef(*new WebXRReferenceSpace(document, WTFMove(session), WTFMove(offset), type));
}

WebXRReferenceSpace::WebXRReferenceSpace(Document& document, Ref<WebXRSession>&& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
    : WebXRSpace(document, WTFMove(session), WTFMove(offset))
    , m_type(type)
{
}

WebXRReferenceSpace::~WebXRReferenceSpace() = default;

TransformationMatrix WebXRReferenceSpace::nativeOrigin() const {
    TransformationMatrix origin;

    // We assume that poses got from the devices are in local space.
    // This will require more complex logic if we add ports with different default coordinates.
    switch (m_type) {
    case XRReferenceSpaceType::Viewer: {
        // Return the current pose. Content rendered in viewer pose will stay in fixed point on HMDs.
        const PlatformXR::Device::FrameData& data = m_session->animationFrame().getFrameData();
        origin.translate3d(data.origin.position.x(), data.origin.position.y(), data.origin.position.z());
        origin *= TransformationMatrix::fromQuaternion(data.origin.orientation.x, data.origin.orientation.y, data.origin.orientation.z, data.origin.orientation.w);
        break;
    }
    case XRReferenceSpaceType::Local:
    case XRReferenceSpaceType::Unbounded:
        // Local and unbounded use the same device space, use the identity matrix.
        break;
    case XRReferenceSpaceType::LocalFloor:
        // TODO: 
        break;
    case XRReferenceSpaceType::BoundedFloor:
    default:
        // BoundedFloor is handled by WebXRBoundedReferenceSpace subclass
        RELEASE_ASSERT_NOT_REACHED();
    }

    return origin;
}

RefPtr<WebXRReferenceSpace> WebXRReferenceSpace::getOffsetReferenceSpace(const WebXRRigidTransform& offsetTransform)
{
    if (!scriptExecutionContext())
        return nullptr;
    ASSERT(is<Document>(scriptExecutionContext()));

    // https://immersive-web.github.io/webxr/#dom-xrreferencespace-getoffsetreferencespace
    // Set offsetSpace’s origin offset to the result of multiplying base’s origin offset by originOffset in the relevant realm of base.
    Ref<WebXRRigidTransform> offset = WebXRRigidTransform::create(m_originOffset->rawTransform() * offsetTransform.rawTransform());

    return create(downcast<Document>(*scriptExecutionContext()), m_session.copyRef(), WTFMove(offset), m_type);
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
