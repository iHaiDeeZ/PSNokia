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

#include <midpport_eventqueue.h>

#include <midp_logging.h>

#ifdef PS4
#include <pthread.h>
#else
#include <psp2/kernel/threadmgr.h>
#endif

/**
 * @file
 *
 * Platform specific system services, such as event handling.
 */

/*=========================================================================
 * Event handling functions
 *=======================================================================*/

/* This port's Java threads are all cooperatively scheduled on a single
 * native OS thread (see the original startup-freeze investigation), so
 * these were previously safe no-ops in practice. Given a real, cheap
 * implementation is available, use one anyway rather than relying on
 * that architectural assumption never changing. */
#ifdef PS4
static pthread_mutex_t event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Create the event queue lock. */
void
midp_createEventQueueLock(void) {
}

/** Destroy the event queue lock. */
void
midp_destroyEventQueueLock(void) {
}

/** Wait to get the event queue lock and then lock it. */
void
midp_waitAndLockEventQueue(void) {
    pthread_mutex_lock(&event_queue_mutex);
}

/** Unlock the event queue. */
void
midp_unlockEventQueue(void) {
    pthread_mutex_unlock(&event_queue_mutex);
}
#else
static SceUID event_queue_mutex = -1;

/** Create the event queue lock. */
void
midp_createEventQueueLock(void) {
    event_queue_mutex = sceKernelCreateMutex("MidpEventQueueLock", 0, 0, NULL);
    if (event_queue_mutex < 0) {
        REPORT_WARN(LC_EVENTS, "midp_createEventQueueLock: sceKernelCreateMutex failed.");
    }
}

/** Destroy the event queue lock. */
void
midp_destroyEventQueueLock(void) {
    if (event_queue_mutex >= 0) {
        sceKernelDeleteMutex(event_queue_mutex);
        event_queue_mutex = -1;
    }
}

/** Wait to get the event queue lock and then lock it. */
void
midp_waitAndLockEventQueue(void) {
    if (event_queue_mutex >= 0) {
        sceKernelLockMutex(event_queue_mutex, 1, NULL);
    }
}

/** Unlock the event queue. */
void
midp_unlockEventQueue(void) {
    if (event_queue_mutex >= 0) {
        sceKernelUnlockMutex(event_queue_mutex, 1);
    }
}
#endif
