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
package javax.microedition.media;

import java.io.InputStream;
import java.io.IOException;

/**
 * Vita port's real Manager: tone playback (Manager.playTone), tone
 * sequences (Manager.TONE_DEVICE_LOCATOR), the live MIDI device
 * (Manager.MIDI_DEVICE_LOCATOR / MIDIPlayer - real-time note on/off via
 * MIDIControl.shortMidiEvent, synthesized with plain tones rather than
 * real GM instrument samples, see MIDIPlayer's class doc), and WAV
 * sample playback (GenericPlayer) are all wired to a real native
 * SDL2-backed mixer. Standard MIDI Files (createPlayer(InputStream,
 * "audio/midi")) play through MidiFilePlayer's software synthesizer.
 */
public final class Manager {
    static private native void nPlayTone(int note, int duration, int volume);

    public final static String TONE_DEVICE_LOCATOR = "device://tone";
    public final static String MIDI_DEVICE_LOCATOR = "device://midi";

    private final static String PL_ERR = "Cannot create a Player for: ";

    private Manager() { }

    public static String[] getSupportedContentTypes(String protocol) {
        return new String[0];
    }

    public static String[] getSupportedProtocols(String content_type) {
        return new String[0];
    }

    public static Player createPlayer(String locator) throws IOException, MediaException {
        Player ret = null;
        if (locator == null) { throw new IllegalArgumentException(); }
        /* catch(Throwable), not catch(Exception): a content type this
         * build doesn't have a concrete Player class for (MIDI) throws
         * NoClassDefFoundError - an Error, not an Exception. Left
         * uncaught, that can hang a caller waiting on a background
         * loading thread that dies silently instead of getting the
         * normal, expected MediaException it already knows how to
         * handle - see the matching comment in the InputStream overload
         * below, and build notes gotcha on this exact failure shape. */
        try {
            if (locator.compareTo(TONE_DEVICE_LOCATOR) == 0) {
                ret = new ToneSequencePlayer();
            } else if (locator.compareTo(MIDI_DEVICE_LOCATOR) == 0) {
                ret = new MIDIPlayer();
            } else {
                ret = null; // no generic-locator (file/http/...) support yet
            }
        } catch (Throwable e) { }
        if (ret == null) { throw new MediaException(PL_ERR + locator); }
        return ret;
    }

    public static Player createPlayer(InputStream stream, String type) throws IOException, MediaException {
        ABBBasicPlayer ret;
        if (stream == null) { throw new IllegalArgumentException(); }
        if (type == null) { throw new MediaException(PL_ERR + "NULL content-type"); }
        try {
            // audio/midi, audio/mid, audio/x-midi, audio/sp-midi, ...
            if (type.startsWith("audio/") && type.indexOf("mid") >= 0) {
                ret = new MidiFilePlayer();
            } else {
                ret = new GenericPlayer(type);
            }
        } catch (Throwable e) {
            throw new MediaException("Media type not supported: " + type);
        }
        ret.setSource(stream);
        return ret;
    }

    public static void playTone(int note, int duration, int volume) throws MediaException {
        if (note < 0 || note > 127 || duration <= 0) { throw new IllegalArgumentException("bad param"); }
        if (volume < 0) { volume = 0; }
        if (volume > 100) { volume = 100; }
        nPlayTone(note, duration, volume);
    }
}
