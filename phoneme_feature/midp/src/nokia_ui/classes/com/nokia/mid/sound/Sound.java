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
package com.nokia.mid.sound;

import java.io.ByteArrayInputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;

/**
 * Nokia UI API (com.nokia.mid.sound.Sound) - a proprietary, pre-MMAPI
 * sound API some older Nokia S40-originated MIDlets use directly instead
 * of (or alongside) javax.microedition.media, distinct from the
 * com.nokia.mid.ui package this port already stubs. Real WAV data
 * (RIFF/WAVE-tagged, FORMAT_WAV) is played for real via the existing
 * javax.microedition.media pipeline (Manager.createPlayer + GenericPlayer
 * - see build notes). Anything else (FORMAT_TONE, or any data this port
 * doesn't recognize as real WAV - e.g. Nokia's own compact proprietary
 * tone-data format, not the same as MIDP's standard tone-sequence format
 * this port already parses) falls back to a short representative beep
 * via Manager.playTone() rather than silently doing nothing - this
 * port does not reverse-engineer Nokia's proprietary tone encoding, so
 * playback in that case is not the intended melody, just an audible cue
 * instead of true silence.
 */
public class Sound {
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 4;

    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 2;

    private static final int FALLBACK_NOTE = 72;
    private static final int FALLBACK_DURATION_MS = 120;

    private final int type;
    private int gain = 100;
    private int state = SOUND_UNINITIALIZED;
    private SoundListener listener;
    private Player player; // non-null only for a real, WAV-backed sound
    private Thread fallbackTimer;

    public Sound(byte[] data, int type) {
        this.type = type;
        if (type == FORMAT_WAV && looksLikeWav(data)) {
            try {
                Player p = Manager.createPlayer(new ByteArrayInputStream(data), "audio/x-wav");
                p.realize();
                p.prefetch();
                p.addPlayerListener(new PlayerListener() {
                    public void playerUpdate(Player pl, String event, Object eventData) {
                        if (event.equals(PlayerListener.END_OF_MEDIA)) {
                            setState(SOUND_STOPPED);
                        }
                    }
                });
                this.player = p;
            } catch (Throwable e) {
                this.player = null;
            }
        }
        this.state = SOUND_STOPPED;
    }

    private static boolean looksLikeWav(byte[] data) {
        return data != null && data.length > 12
            && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F'
            && data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E';
    }

    public static int getSupportedFormats() {
        return FORMAT_TONE | FORMAT_WAV;
    }

    public static boolean isFormatSupported(int format) {
        return format == FORMAT_TONE || format == FORMAT_WAV;
    }

    public void play(int loops) {
        setState(SOUND_PLAYING);
        if (player != null) {
            try {
                player.setLoopCount(loops <= 0 ? -1 : loops);
                player.start();
            } catch (Throwable e) {
                setState(SOUND_STOPPED);
            }
            return;
        }
        try {
            Manager.playTone(FALLBACK_NOTE, FALLBACK_DURATION_MS, gain);
        } catch (Throwable e) { }
        if (fallbackTimer != null) { return; }
        fallbackTimer = new Thread(new Runnable() {
            public void run() {
                try { Thread.sleep(FALLBACK_DURATION_MS); } catch (InterruptedException e) { }
                fallbackTimer = null;
                setState(SOUND_STOPPED);
            }
        });
        fallbackTimer.start();
    }

    public void stop() {
        if (player != null) {
            try { player.stop(); } catch (Throwable e) { }
        }
        setState(SOUND_STOPPED);
    }

    public void resume() {
        if (player != null) {
            try { player.start(); setState(SOUND_PLAYING); } catch (Throwable e) { }
        }
    }

    public void release() {
        if (player != null) {
            try { player.close(); } catch (Throwable e) { }
            player = null;
        }
        setState(SOUND_UNINITIALIZED);
    }

    public int getState() {
        return state;
    }

    public int getGain() {
        return gain;
    }

    public void setGain(int gain) {
        if (gain < 0) { gain = 0; }
        if (gain > 100) { gain = 100; }
        this.gain = gain;
    }

    public void setSoundListener(SoundListener listener) {
        this.listener = listener;
    }

    private void setState(int newState) {
        state = newState;
        if (listener != null) {
            listener.soundStateChanged(this, newState);
        }
    }
}
