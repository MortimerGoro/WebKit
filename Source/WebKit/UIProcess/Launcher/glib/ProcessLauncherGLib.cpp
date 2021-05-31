/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessLauncher.h"

#include "BubblewrapLauncher.h"
#include "Connection.h"
#include "FlatpakLauncher.h"
#include "ProcessExecutablePath.h"
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#include <dlfcn.h>
#include <jni.h>

namespace WebKit {

#if OS(LINUX)
static bool isFlatpakSpawnUsable()
{
    static std::optional<bool> ret;
    if (ret)
        return *ret;

    // For our usage to work we need flatpak >= 1.5.2 on the host and flatpak-xdg-utils > 1.0.1 in the sandbox
    GRefPtr<GSubprocess> process = adoptGRef(g_subprocess_new(static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE),
        nullptr, "flatpak-spawn", "--sandbox", "--sandbox-expose-path-ro-try=/this_path_doesnt_exist", "echo", nullptr));

    if (!process.get())
        ret = false;
    else
        ret = g_subprocess_wait_check(process.get(), nullptr, nullptr);

    return *ret;
}
#endif

#if ENABLE(BUBBLEWRAP_SANDBOX)
static bool isInsideDocker()
{
    static std::optional<bool> ret;
    if (ret)
        return *ret;

    ret = g_file_test("/.dockerenv", G_FILE_TEST_EXISTS);
    return *ret;
}

static bool isInsideFlatpak()
{
    static std::optional<bool> ret;
    if (ret)
        return *ret;

    ret = g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
    return *ret;
}

static bool isInsideSnap()
{
    static std::optional<bool> ret;
    if (ret)
        return *ret;

    // The "SNAP" environment variable is not unlikely to be set for/by something other
    // than Snap, so check a couple of additional variables to avoid false positives.
    // See: https://snapcraft.io/docs/environment-variables
    ret = g_getenv("SNAP") && g_getenv("SNAP_NAME") && g_getenv("SNAP_REVISION");
    return *ret;
}
#endif

void ProcessLauncher::launchProcess()
{
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection(IPC::Connection::ConnectionOptions::SetCloexecOnServer);

    // Android prohibits the forking syscall on non-rooted devices, so we need to
    // provide separate services equivalent to WPEWebProcess and WPENetworkProcess
    // that are spawned from the Java part.
    WTFLogAlways("ProcessLauncher::launchProcess() processType %d\n",
                 m_launchOptions.processType);

    JNIEnv* jniEnv = *reinterpret_cast<JNIEnv**>(dlsym(RTLD_DEFAULT, "s_WPEUIProcessGlue_env"));
    jobject jniObj = *reinterpret_cast<jobject*>(dlsym(RTLD_DEFAULT, "s_WPEUIProcessGlue_object"));
    {
        jclass jClass = jniEnv->GetObjectClass(jniObj);
        WTFLogAlways("  jClass for com/wpe/wpe/WPEUIProcessGlue %p", jClass);
        jmethodID jMethodID = jniEnv->GetMethodID(jClass, "launchProcess", "(I[I)V");
        WTFLogAlways("  jMethodID for launchProcess %p", jMethodID);

        jintArray fdArray = jniEnv->NewIntArray(2);
        int fdArrayValues[2] = { socketPair.client, -1 };
        jniEnv->SetIntArrayRegion(fdArray, 0, 2, fdArrayValues);


        jniEnv->CallVoidMethod(jniObj, jMethodID, static_cast<int>(m_launchOptions.processType), fdArray);

        jniEnv->DeleteLocalRef(fdArray);
        jniEnv->DeleteLocalRef(jClass);
    }

    // We've finished launching the process, message back to the main run loop.
    RunLoop::main().dispatch([protectedThis = makeRef(*this), this, serverSocket = socketPair.server] {
        didFinishLaunchingProcess(m_processIdentifier, serverSocket);
    });
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        invalidate();
        return;
    }

    // TODO: Call into the Android layer to terminate the equivalent service.
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
