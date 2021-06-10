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
#include <android/hardware_buffer.h>
#include <WebCore/PlatformXR.h>

constexpr uint32_t poolSize = 3; 

using namespace WebCore;

namespace WebKit {

std::unique_ptr<XRHardwareBuffer> XRHardwareBuffer::create(JNIEnv* env, uint32_t width, uint32_t height, bool alpha)
{
    auto buffer = std::unique_ptr<XRHardwareBuffer>(new XRHardwareBuffer(env, width, height, alpha));
    if (!buffer->initialize())
        return nullptr;

    return buffer;
}

XRHardwareBuffer::XRHardwareBuffer(JNIEnv* env, uint32_t width, uint32_t height, bool alpha)
    : m_env(env)
    , m_width(width)
    , m_height(height)
    , m_alpha(alpha)
{
}

XRHardwareBuffer::~XRHardwareBuffer()
{
    for (auto buffer: m_pool) {
        AHardwareBuffer_release(buffer);
    }

    m_pool.clear();
}

bool XRHardwareBuffer::initialize()
{
    AHardwareBuffer_Desc desc { };
    desc.width = m_width;
    desc.height = m_height;
    desc.format = m_alpha ? AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM : AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    desc.layers = 1;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER;
    for (uint32_t i = 0; i < poolSize; ++i) {
        AHardwareBuffer* buffer { nullptr };
        
        AHardwareBuffer_allocate(&desc, &buffer);
        if (!buffer)
            return false;

        m_pool.append(buffer);
    }


    return true;
}

PlatformXR::Device::FrameData::LayerData XRHardwareBuffer::startFrame()
{
    ASSERT(!m_frameStarted);
    m_frameStarted = true;
    m_frameCount++;

    auto buffer = m_pool.at(m_poolIndex);

    PlatformXR::Device::FrameData::LayerData data;
    data.hardwareBuffer.handle = m_poolIndex;
    data.hardwareBuffer.buffer = m_pool.at(m_poolIndex);
    // Only serialize textures once, it seems Android gets different instances each time.
    data.hardwareBuffer.reuse = m_frameCount > poolSize;

    return data;
}

AHardwareBuffer* XRHardwareBuffer::endFrame()
{
    ASSERT(m_frameStarted);
    auto result = m_pool.at(m_poolIndex);
    m_poolIndex = (m_poolIndex + 1) % poolSize;
    m_frameStarted = false;

    return result;
}


} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
