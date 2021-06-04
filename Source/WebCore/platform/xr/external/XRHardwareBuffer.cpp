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
#include "XRHardwareBuffer.h"
#include <hardware_buffer_jni.h>

constexpr uint32_t poolSize = 3; 

using namespace WebCore;

namespace PlatformXR {

std::unique_ptr<XRHardwareBuffer> XRHardwareBuffer::create(WebCore::GLContextEGL& egl, WebCore::GraphicsContextGL& gl, uint32_t width, uint32_t height)
{
    auto buffer = std::unique_ptr<XRHardwareBuffer>(new XRHardwareBuffer(egl, gl, width, height));
    if (!buffer->initialize())
        return nullptr;

    return buffer;
}

XRHardwareBuffer::XRHardwareBuffer(WebCore::GLContextEGL& egl, WebCore::GraphicsContextGL& gl, uint32_t width, uint32_t height)
    : m_egl(egl)
    , m_gl(gl)
    , m_width(width)
    , m_height(height)
{
}

XRHardwareBuffer::~XRHardwareBuffer()
{
    for (auto& buffer: m_pool) {
        if (buffer.texture)
            m_gl.deleteTexture(buffer.texture);
        if (buffer.image != EGL_NO_IMAGE_KHR)
            m_egl.destroyImageKHR(PlatformDisplay::sharedDisplay().eglDisplay(), buffer.image);
        if (buffer.hardwareBuffer)
            AHardwareBuffer_release(buffer.hardwareBuffer);
    }

    m_pool.clear();
}

bool XRHardwareBuffer::initialize()
{
    AHardwareBuffer_Desc desc { };
    desc.width = m_width
    desc.height = m_height;
    desc.layers = 1;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER;
    for (int i = 0; i < poolSize; ++i) {
        Buffer buffer { };
        
        AHardwareBuffer_allocate(&desc, &buffer.hardwareBuffer);
        if (!buffer.hardwareBuffer)
            return false;

        buffer.clientBuffer = m_egl.getNativeClientBufferANDROID(hardwareBuffer);
        if (!buffer.clientBuffer)
            return false;

        buffer.image = m_egl.createImageKHR(PlatformDisplay::sharedDisplay().eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buffer.clientBuffer, nullptr);
        if (buffer.image == EGL_NO_IMAGE_KHR)
            return false;

        buffer.texture = m_gl.createTexture();
        m_gl.bindTexture(GL_TEXTURE_2D, buffer.texture);
        m_egl.imageTargetTexture2DOES(GL_TEXTURE_2D, buffer.image);

        buffer.javaObject = AHardwareBuffer_toHardwareBuffer

        m_pool.append(WTFMove(buffer));
    }
}

Device::FrameData::LayerData XRHardwareBuffer::startFrame()
{
    ASSERT(!m_frameStarted);
    Device::FrameData::LayerData data { };
    data.opaqueTexture = m_pool.get(m_poolIndex).texture;

    m_frameStarted = true;

    return data;
}

jobject XRHardwareBuffer::endFrame()
{
    ASSERT(m_frameStarted);
    m_poolIndex = (m_poolIndex + 1) % poolSize;
    m_frameStarted = false;


}


} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
