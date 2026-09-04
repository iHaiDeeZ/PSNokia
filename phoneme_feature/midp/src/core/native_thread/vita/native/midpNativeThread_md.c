/*
 *
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */
#include <stdio.h>
#include <kni.h>
#include <jvm.h>
#include <sni.h>
#include <midp_logging.h>
#include <midp_constants_data.h>
#include <midpNativeThread.h>
#include <psp2/kernel/threadmgr.h>
/**
 * @file
 *
 * Vita/vitasdk implementation of native thread services.
 */
#if ENABLE_NATIVE_AMS && ENABLE_I3_TEST
/**
 * starts another native thread.
 *
 * ATTENTION: this is a stub, matching upstream sdl/wince ports -
 * NAMS/I3_TEST are not enabled in this build.
 *
 * @param thread thread routine
 * @param param thread routine parameter
 *
 * @return handle of created thread
 */
midp_ThreadId midp_startNativeThread(midp_ThreadRoutine thread,
    midp_ThreadRoutineParameter param) {
    (void)thread;
    (void)param;
    REPORT_WARN(LC_AMS, "midp_startNativeThread: Stubbed out.");
    return MIDP_INVALID_NATIVE_THREAD_ID;
}
#endif
/**
 * suspends current thread for a given number of seconds.
 *
 * @param duration  how many seconds to sleep
 */
void midp_sleepNativeThread(int duration) {
    if (duration > 0) {
        sceKernelDelayThread((SceUInt)duration * 1000000);
    }
}
/**
 * Returns the platform-specific handle of the current thread.
 *
 * @return handle of the current created thread
 */
midp_ThreadId midp_getCurrentThreadId() {
    return (midp_ThreadId)(long)sceKernelGetThreadId();
}
