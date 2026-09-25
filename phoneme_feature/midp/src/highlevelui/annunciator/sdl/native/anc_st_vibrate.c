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

#include <kni.h>
#include <anc_vibrate.h>
#include <midp_logging.h>

/**
 * @file
 *
 * @brief Native code to handle vibrate control
 *
 * @note If the target platform does not have vibrate capability 
 * then there is no need to modify this file. 
 */

/**
 * Platform dependent implementation of startVibrate
 *
 * @note start vibrate is not implemented, as planned.
 * @parameter dur duration of the vibrate period in 
 *            microseconds
 * @return KNI_FALSE:  if this device does not support vibrate
 */
#ifdef PS4
/* DS4 rumble, in midp_msgQueue_md.c; level 0..255 */
void ps4_vibrate(int level, int ms);
#endif

jboolean anc_start_vibrate(int dur)
{
    REPORT_CALL_TRACE1(LC_CORE, "LF:anc_start_vibrate(%d)\n", dur);
#ifdef PS4
    ps4_vibrate(200, dur);
    return KNI_TRUE;
#else
    (void)dur;
    return KNI_FALSE;
#endif
}

/**
 * Platform dependent implementation of stopVibrate.
 *
 * @return KNI_FALSE: if this device does not support vibrate
 */
jboolean anc_stop_vibrate(void)
{
    REPORT_CALL_TRACE(LC_CORE, "LF:anc_stop_vibrate()\n");
#ifdef PS4
    ps4_vibrate(0, 0);
    return KNI_TRUE;
#else
    return KNI_FALSE;
#endif
}

/*
 * com.nokia.mid.ui.DeviceControl.nVibrate(int freq, long duration): Nokia
 * games' vibration (freq 0..100 is the strength; 0 stops). Returns whether
 * the device can vibrate.
 */
KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_com_nokia_mid_ui_DeviceControl_nVibrate() {
    jint freq = KNI_GetParameterAsInt(1);
    jlong duration = KNI_GetParameterAsLong(2);
#ifdef PS4
    if (freq < 0) {
        KNI_ReturnBoolean(KNI_TRUE); /* support query only */
    }
    if (freq > 100) freq = 100;
    if (duration > 10000) duration = 10000;
    ps4_vibrate(freq > 0 ? 80 + freq * 175 / 100 : 0, (int)duration);
    KNI_ReturnBoolean(KNI_TRUE);
#else
    (void)freq;
    (void)duration;
    KNI_ReturnBoolean(KNI_FALSE);
#endif
}
