/*
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
package com.nokia.mid.ui;

/**
 * Nokia UI API (com.nokia.mid.ui.DeviceControl) - backlight/vibra
 * control. Added preemptively alongside the com.nokia.mid.sound.Sound
 * stub after finding a real MIDlet referencing it (unconfirmed whether
 * actually called) - a genuine no-op stub here (rather than no class at
 * all) is what turns a possible class-resolution crash into a harmless,
 * silent no-op, matching this port's existing com.nokia.mid.ui stub
 * approach. Not wired to any real Vita backlight/vibration control.
 */
public class DeviceControl {
    private DeviceControl() { }

    public static void setLights(int num, int level) {
    }

    public static boolean isLightSupported(int num) {
        return false;
    }

    public static void startVibra(int freq, long duration) {
    }

    public static void stopVibra() {
    }

    public static boolean isVibraSupported() {
        return false;
    }
}
