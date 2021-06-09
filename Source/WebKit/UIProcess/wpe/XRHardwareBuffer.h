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

#include <jni.h>
#include <wtf/Vector.h>

class AHardwareBuffer;
namespace WebKit {

class XRHardwareBuffer {
public:
    static std::unique_ptr<XRHardwareBuffer> create(JNIEnv*, uint32_t width, uint32_t height, bool alpha);
    ~XRHardwareBuffer();

    PlatformXR::Device::FrameData::LayerData startFrame();
    void endFrame();

private:
    XRHardwareBuffer(JNIEnv*, uint32_t width, uint32_t height, bool alpha);
    bool initialize();

    JNIEnv* m_env { nullptr };
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    bool m_alpha { false };
    Vector<AHardwareBuffer*> m_pool;
    int m_poolIndex { 0 };
    bool m_frameStarted { false };
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
