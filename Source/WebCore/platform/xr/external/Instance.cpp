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

#include "PlatformXRExternal.h"

#include <wtf/Scope.h>

#include <android/log.h>
#define XR_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "PlatformXR::ExternalDevice", __VA_ARGS__)
#define XR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PlatformXR::ExternalDevice", __VA_ARGS__)

using namespace WebCore;

namespace PlatformXR {

struct Instance::Impl {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    Impl();
    ~Impl();

    WorkQueue& queue() const { return m_workQueue; }

private:
    Ref<WorkQueue> m_workQueue;
};

Instance::Impl::Impl()
    : m_workQueue(WorkQueue::create("ExternalXR queue"))
{
    XR_LOGV("Create instance");
}

Instance::Impl::~Impl()
{
}

Instance& Instance::singleton()
{
    static LazyNeverDestroyed<Instance> s_instance;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [&] {
            s_instance.construct();
        });
    return s_instance.get();
}

Instance::Instance()
    : m_impl(makeUniqueRef<Impl>())
{
}

void Instance::enumerateImmersiveXRDevices(CompletionHandler<void(const DeviceList& devices)>&& callback)
{
    XR_LOGV("enumerateImmersiveXRDevices");
    callOnMainThread([this, callback = WTFMove(callback)]() mutable {
        m_immersiveXRDevices = DeviceList::from(ExternalDevice::create(nullptr, m_impl->queue(), [this, callback = WTFMove(callback)]() mutable {
            ASSERT(isMainThread());
            XR_LOGV("ExternalDevice created");
            callback(m_immersiveXRDevices);
        }));
    });
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(EXTERNALXR)
