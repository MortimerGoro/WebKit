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
#include "WebFakeXRDevice.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "JSDOMPromiseDeferred.h"
#include "WebFakeXRInputController.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MathExtras.h>

namespace WebCore {

void FakeXRView::setProjection(const Vector<float>& projection) {
    std::copy(std::begin(projection), std::end(projection), std::begin(m_projection));
}

void FakeXRView::setFieldOfView(const FakeXRViewInit::FieldOfViewInit& fov)
{
    m_fov = PlatformXR::Device::FrameData::Fov { deg2rad(fov.upDegrees), deg2rad(fov.downDegrees), deg2rad(fov.leftDegrees), deg2rad(fov.rightDegrees) };
}

WebFakeXRDevice::WebFakeXRDevice() = default;

void WebFakeXRDevice::setViews(const Vector<FakeXRViewInit>& views)
{
    m_device.scheduleOnNextFrame([&](){
        Vector<Ref<FakeXRView>>& deviceViews = m_device.views();
        deviceViews.clear();

        for (auto& viewInit : views) {
            auto view = parseView(viewInit);
            if (!view.hasException())
                deviceViews.append(view.releaseReturnValue());
        }
    });
}

void WebFakeXRDevice::disconnect(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void WebFakeXRDevice::setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition)
{
    auto rigidTransform = parseRigidTransform(origin);
    if (rigidTransform.hasException())
        return;

    RefPtr<WebXRRigidTransform> transform = rigidTransform.releaseReturnValue();

    m_device.scheduleOnNextFrame([=](){
        m_device.setViewerOrigin(transform);
        m_device.setEmulatedPosition(emulatedPosition);
    });
}

void WebFakeXRDevice::clearViewerOrigin()
{
    m_device.scheduleOnNextFrame([&](){
        m_device.setViewerOrigin(nullptr);
    });
}

void WebFakeXRDevice::simulateVisibilityChange(XRVisibilityState)
{
}

void WebFakeXRDevice::setBoundsGeometry(Vector<FakeXRBoundsPoint>)
{
}

void WebFakeXRDevice::setFloorOrigin(FakeXRRigidTransformInit origin)
{
    auto rigidTransform = parseRigidTransform(origin);
    if (rigidTransform.hasException())
        return;

    RefPtr<WebXRRigidTransform> transform = rigidTransform.releaseReturnValue();

    m_device.scheduleOnNextFrame([&](){
        m_device.setFloorOrigin(transform);
    });
}

void WebFakeXRDevice::clearFloorOrigin()
{
    m_device.scheduleOnNextFrame([&](){
        m_device.setFloorOrigin(nullptr);
    });
}

void WebFakeXRDevice::simulateResetPose()
{
}



Ref<WebFakeXRInputController> WebFakeXRDevice::simulateInputSourceConnection(FakeXRInputSourceInit)
{
    return WebFakeXRInputController::create();
}

ExceptionOr<Ref<WebXRRigidTransform>> WebFakeXRDevice::parseRigidTransform(const FakeXRRigidTransformInit& init)
{
    if (init.position.size() != 3 || init.orientation.size() != 4)
        return Exception { TypeError };

    DOMPointInit position;
    position.x = init.position[0];
    position.y = init.position[1];
    position.z = init.position[2];

    DOMPointInit orientation;
    orientation.x = init.orientation[0];
    orientation.y = init.orientation[1];
    orientation.z = init.orientation[2];
    orientation.w = init.orientation[3];

    return WebXRRigidTransform::create(position, orientation);
}

ExceptionOr<Ref<FakeXRView>> WebFakeXRDevice::parseView(const FakeXRViewInit& init)
{
    // https://immersive-web.github.io/webxr-test-api/#parse-a-view
    auto fakeView = FakeXRView::create(init.eye);

    if (init.projectionMatrix.size() != 16)
        return Exception { TypeError };
    fakeView->setProjection(init.projectionMatrix);

    auto viewOffset = parseRigidTransform(init.viewOffset);
    if (viewOffset.hasException())
        return viewOffset.releaseException();
    fakeView->setOffset(viewOffset.releaseReturnValue());

    fakeView->setResolution(init.resolution);

    if (init.fieldOfView) {
        fakeView->setFieldOfView(init.fieldOfView.value());
    }

    return fakeView;
}

SimulatedXRDevice::SimulatedXRDevice()
    : m_frameTimer(*this, &SimulatedXRDevice::frameTimerFired)
{
    m_supportsOrientationTracking = true;
}

SimulatedXRDevice::~SimulatedXRDevice()
{
    m_frameTimer.stop();
}

void SimulatedXRDevice::frameTimerFired()
{
    Vector<Function<void()>> pendingUpdates;
    pendingUpdates.swap(m_pendingUpdates);
    for (auto& update : pendingUpdates)
        update();

    FrameData data = {};
    if (m_viewerOrigin) {
        auto& position = m_viewerOrigin->position();
        auto& orientation = m_viewerOrigin->orientation();
        data.origin.position = { (float)position.x(), (float) position.y(), (float) position.z() };
        data.origin.orientation = { (float)orientation.x(), (float)orientation.y(), (float)orientation.z(), (float)orientation.w() };
        data.isTrackingValid = true;
        data.isPositionValid = true;
    }

    for (auto& view: m_views) {
        FrameData::View pose = {};
        auto& position = view->offset()->position();
        auto& orientation = view->offset()->orientation();
        pose.offset.position = { (float)position.x(), (float)position.y(), (float)position.z() };
        pose.offset.orientation = { (float)orientation.x(), (float)orientation.y(), (float)orientation.z(), (float)orientation.w() };
        if (view->fieldOfView().hasValue()) {
            pose.projection = { *view->fieldOfView() };
        } else {
            pose.projection = { view->projection() };
        }
        
        data.views.append(pose);
    }

    Vector<RequestFrameCallback> runningCallbacks;
    runningCallbacks.swap(m_callbacks);
    for (auto& callback : runningCallbacks)
        callback(data);
}

void SimulatedXRDevice::initializeTrackingAndRendering(PlatformXR::SessionMode)
{
}

Vector<PlatformXR::Device::ViewData> SimulatedXRDevice::views(PlatformXR::SessionMode mode) const {
    if (mode == PlatformXR::SessionMode::ImmersiveVr) {
        return { { .active = true, PlatformXR::Eye::Left }, { .active = true, PlatformXR::Eye::Right } };
    } else {
        return { { .active = true, PlatformXR::Eye::None } };
    }
}

void SimulatedXRDevice::requestFrame(RequestFrameCallback&& callback)
{
    m_callbacks.append(WTFMove(callback));
    if (!m_frameTimer.isActive())
        m_frameTimer.startOneShot(15_ms);
}

void SimulatedXRDevice::shutDownTrackingAndRendering()
{
    if (m_frameTimer.isActive())
        m_frameTimer.stop();
}

void SimulatedXRDevice::scheduleOnNextFrame(Function<void()>&& func)
{
    func();
    //m_pendingUpdates.append(WTFMove(func));
}


} // namespace WebCore

#endif // ENABLE(WEBXR)
