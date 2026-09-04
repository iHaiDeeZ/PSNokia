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

/**
 * @file
 *
 * Functions that enable MIDP to retrieve platform timezone setting.
 */

#include <stdio.h>

#include <midp_logging.h>

/**
 * Return local timezone.
 *
 * @return Local timezone ID string pointer. The ID string should be in the
 *	   format of GMT+/-??:??. For example, GMT-08:00 for PST.
 */

#include <midp_properties_port.h>
#include <midpTimeZone.h>

#include <psp2/rtc.h>

/**
 * Return local timezone ID string. This string is maintained by this
 * function internally. Caller must NOT try to free it.
 *
 * This function should handle daylight saving time properly. For example,
 * for time zone America/Los_Angeles, during summer time, this function
 * should return GMT-07:00 and GMT-08:00 during winter time.
 *
 * @return Local timezone ID string pointer. The ID string should be in the
 *         format of GMT+/-??:??. For example, GMT-08:00 for PST.
 */
char* getLocalTimeZone() {
    static char tz[12]; /* No longer than "GMT-10:00" */
    SceDateTime utcTime, localTime;
    SceRtcTick utcTick, localTick;
    int offsetMinutes = 0;

    /* sceRtcGetCurrentClockLocalTime() already applies the Vita's
     * configured system timezone (and DST, if the underlying platform
     * clock accounts for it) - compute the real offset by diffing it
     * against UTC, instead of hardcoding +00:00 regardless of the
     * device's actual setting. */
    if (sceRtcGetCurrentClock(&utcTime, 0) >= 0 &&
        sceRtcGetCurrentClockLocalTime(&localTime) >= 0 &&
        sceRtcGetTick(&utcTime, &utcTick) >= 0 &&
        sceRtcGetTick(&localTime, &localTick) >= 0) {
        SceInt64 diffUs = (SceInt64)localTick.tick - (SceInt64)utcTick.tick;
        offsetMinutes = (int)(diffUs / (60LL * 1000000LL));
    } else {
        REPORT_WARN(LC_CORE, "getLocalTimeZone: RTC query failed, defaulting to GMT+00:00.");
    }

    {
        int hours = offsetMinutes / 60;
        int mins = offsetMinutes % 60;
        if (mins < 0) {
            mins = -mins;
        }
        sprintf(tz, "GMT%+03d:%02d", hours, mins);
    }

    return tz;
}
