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
#include "WebXRFrame.h"

#if ENABLE(WEBXR)

#include "WebXRBoundedReferenceSpace.h"
#include "WebXRReferenceSpace.h"
#include "WebXRSession.h"
#include "WebXRViewerPose.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRFrame);

Ref<WebXRFrame> WebXRFrame::create(WebXRSession& session, bool isAnimationFrame)
{
    return adoptRef(*new WebXRFrame(session, isAnimationFrame));
}

WebXRFrame::WebXRFrame(WebXRSession& session, bool isAnimationFrame)
    : m_active(false)
    , m_animationFrame(isAnimationFrame)
    , m_session(session)
{
}

WebXRFrame::~WebXRFrame() = default;

// https://immersive-web.github.io/webxr/#poses-must-be-limited
bool WebXRFrame::mustPosesBeLimited(const WebXRSpace& space, const WebXRSpace& baseSpace)
{
    auto isOutsideNativeBoundsOfBoundedReferenceSpace = [] (const WebXRSpace& space, const WebXRSpace& other) {
        if (!is<WebXRBoundedReferenceSpace>(space))
            return false;

        // TODO: return true whenever the distance from the bounded geometry of
        // |space| to the native origin of |other| space is greater than 1m
        // (suggested by specs).

        return false;
    };

    if (isOutsideNativeBoundsOfBoundedReferenceSpace(space, baseSpace)
        || isOutsideNativeBoundsOfBoundedReferenceSpace(baseSpace, space))
        return true;

    auto isLocalReferenceSpace = [] (const WebXRSpace& space) {
        if (!is<WebXRReferenceSpace>(space))
            return false;

        auto type = downcast<WebXRReferenceSpace>(space).type();
        if (type == XRReferenceSpaceType::Local || type == XRReferenceSpaceType::LocalFloor)
            return true;

        return false;
    };

    if (isLocalReferenceSpace(space) || isLocalReferenceSpace(baseSpace)) {
        // TODO: If the distance between native origins of spaces is greater
        // than 15m (suggested by specs) return true.
    }

    return false;
}

// https://immersive-web.github.io/webxr/#populate-the-pose
ExceptionOr<void> WebXRFrame::populatePose(WebXRPose& pose, const WebXRSpace& space, const WebXRSpace& baseSpace)
{
    // 1. If frame’s active boolean is false, throw an InvalidStateError and abort these steps.
    if (!m_active)
        return Exception { InvalidStateError };

    // 2. Let session be frame’s session object.
    // 3. If space’s session does not equal session, throw an InvalidStateError and abort these steps.
    if (&space.session() != &m_session)
        return Exception { InvalidStateError };

    // 4. If baseSpace’s session does not equal session, throw an InvalidStateError and abort these steps.
    if (&baseSpace.session() != &m_session)
        return Exception { InvalidStateError };

    // 5. Check if poses may be reported and, if not, throw a SecurityError and abort these steps.
    if (!m_session.posesCanBeReported())
        return Exception { SecurityError };

    // 6. Let limit be the result of whether poses must be limited between space and baseSpace.
    bool limit = mustPosesBeLimited(space, baseSpace);

    // 7. Let transform be pose’s transform.
    // 8. Query the XR device's tracking system for space’s pose relative to baseSpace at the frame’s time,
    //    then perform the following steps:
    // TODO: ask platform for data
    return { };
}

// https://immersive-web.github.io/webxr/#dom-xrframe-getviewerpose
ExceptionOr<RefPtr<WebXRViewerPose>> WebXRFrame::getViewerPose(const WebXRReferenceSpace& referenceSpace)
{
    // 1. Let frame be this.
    // 2. Let session be frame’s session object.
    // 3. If frame’s animationFrame boolean is false, throw an InvalidStateError and abort these steps.
    if (!m_animationFrame)
        return Exception { InvalidStateError };

    // 4. Let pose be a new XRViewerPose object in the relevant realm of session.
    RefPtr<WebXRViewerPose> pose = WebXRViewerPose::create();

    // 5. Populate the pose of session’s viewer reference space in referenceSpace at the time represented by frame into pose.
    auto populatePoseResult = populatePose(*pose, m_session.viewerReferenceSpace(), referenceSpace);
    if (populatePoseResult.hasException())
        return populatePoseResult.releaseException();

    // 6. If pose is null return null.
    if (!pose)
        return nullptr;

    // 7. Let xrviews be an empty list.
    Vector<Ref<WebXRView>> xrViews;
    // 8. For each active view view in the list of views on session, perform the following steps:
    for (auto& view : m_session.views()) {
        if (!view.active)
            continue;

        // 8.1 Let xrview be a new XRView object in the relevant realm of session.
        // 8.2 Initialize xrview’s underlying view to view.
        // 8.3 Initialize xrview’s eye to view’s eye.
        // 8.4 Initialize xrview’s frame time to frame’s time.
        // 8.5 Initialize xrview’s session to session.
        auto xrView = WebXRView::create(view, m_time, makeRef(m_session));

        //  8.6. Let offset be an new XRRigidTransform object equal to the view offset of view in the relevant realm of session.
        //  8.7. Set xrview’s transform property to the result of multiplying the XRViewerPose's transform by the offset transform in the relevant realm of session

        //  8.8. Append xrview to xrviews
        xrViews.append(WTFMove(xrView));
    }

    // 9. Set pose’s views to xrviews
    pose->setViews(WTFMove(xrViews));

    // 10. Return pose.
    return pose;
}

RefPtr<WebXRPose> WebXRFrame::getPose(const WebXRSpace&, const WebXRSpace&)
{
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
