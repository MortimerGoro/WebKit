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
#include <wtf/Vector.h>

#include <jni.h>

class AHardwareBuffer;
namespace PlatformXR {

class XRHardwareBuffer {
public:
    static std::unique_ptr<XRHardwareBuffer> create(WebCore::GLContextEGL&, WebCore::GraphicsContextGL&, uint32_t width, uint32_t height);
    ~XRHardwareBuffer();

    Device::FrameData::LayerData startFrame();
    jobject endFrame();

private:
    XRHardwareBuffer(WebCore::GLContextEGL&, WebCore::GraphicsContextGL&, uint32_t width, uint32_t height);
    bool initialize();

    struct Buffer {
        AHardwareBuffer* hardwareBuffer { nullptr };
        EGLClientBuffer clientBuffer { 0 };
        EGLImageKHR image { EGL_NO_IMAGE_KHR };
        PlatformGLObject texture { 0 };
        jobject javaObject { 0 };
    };

    WebCore::GLContextEGL& m_egl;
    WebCore::GraphicsContextGL& m_gl;
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    Vector<Buffer> m_pool;
    int m_poolIndex { 0 };
    bool m_frameStarted { false };
};

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
