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
import java.io.ByteArrayOutputStream;

import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.VolumeControl;

/**
 * Nokia UI API sound (com.nokia.mid.sound.Sound), on top of
 * javax.microedition.media. FORMAT_TONE data is a Nokia Smart Messaging
 * ringtone, converted to a MIDI file here and played by the MIDI
 * synthesizer; FORMAT_WAV plays as WAV; Sound(freq, duration) is a beep.
 */
public class Sound {
    // The Nokia UI API's values: games compile them in, so they must match
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 5;
    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 3;

    private int gain = 100;
    private int state = SOUND_UNINITIALIZED;
    private SoundListener listener;
    private Player player;

    public Sound(int freq, long duration) {
        init(freq, duration);
    }

    public Sound(byte[] data, int type) {
        init(data, type);
    }

    public static int[] getSupportedFormats() {
        return new int[] { FORMAT_TONE, FORMAT_WAV };
    }

    public static int getConcurrentSoundCount(int type) {
        return type == FORMAT_WAV ? 8 : 1;
    }

    public void init(int freq, long duration) {
        if (freq < 0 || duration <= 0) {
            throw new IllegalArgumentException();
        }
        // A single beep, as a one-note MIDI file
        // The MIDI note nearest to freq (CLDC's Math has no log): step
        // semitones from A4 = 440 Hz
        int note = 69;
        double f = 440.0;
        double semitone = 1.0594630943592953;
        while (note < 127 && f * semitone <= freq * 1.0293) {
            f *= semitone;
            note++;
        }
        while (note > 0 && f / semitone >= freq / 1.0293) {
            f /= semitone;
            note--;
        }
        MidiWriter w = new MidiWriter();
        w.note(freq == 0 ? -1 : note, (int) Math.min(duration, 60000), 1.0, 100);
        load(w.toByteArray(), "audio/midi");
    }

    public void init(byte[] data, int type) {
        if (data == null) {
            throw new NullPointerException();
        }
        if (type == FORMAT_WAV) {
            load(data, "audio/x-wav");
        } else if (type == FORMAT_TONE) {
            byte[] midi = isMidi(data) ? data : RingtoneConverter.toMidi(data);
            if (midi == null) {
                throw new IllegalArgumentException("Unsupported tone data");
            }
            load(midi, "audio/midi");
        } else {
            throw new IllegalArgumentException("Unsupported format " + type);
        }
    }

    private static boolean isMidi(byte[] d) {
        return d.length > 4 && d[0] == 'M' && d[1] == 'T' && d[2] == 'h' && d[3] == 'd';
    }

    private void load(byte[] data, String contentType) {
        closePlayer();
        try {
            Player p = Manager.createPlayer(new ByteArrayInputStream(data), contentType);
            p.realize();
            p.prefetch();
            p.addPlayerListener(new PlayerListener() {
                public void playerUpdate(Player pl, String event, Object eventData) {
                    if (pl == player && event == PlayerListener.END_OF_MEDIA) {
                        setState(SOUND_STOPPED);
                    }
                }
            });
            player = p;
            applyGain();
            state = SOUND_STOPPED;
        } catch (Throwable e) {
            System.out.println("Nokia Sound: could not load " + contentType + ": " + e);
            player = null;
            state = SOUND_UNINITIALIZED;
        }
    }

    public void play(int loops) {
        if (loops < 0) {
            throw new IllegalArgumentException();
        }
        if (player == null) {
            return;
        }
        try {
            player.stop();
            player.setMediaTime(0);
        } catch (Throwable e) {
        }
        try {
            // Nokia: 0 loops forever
            player.setLoopCount(loops == 0 ? -1 : loops);
            player.start();
            setState(SOUND_PLAYING);
        } catch (Throwable e) {
            setState(SOUND_STOPPED);
        }
    }

    public void stop() {
        if (player != null) {
            try {
                player.stop();
            } catch (Throwable e) {
            }
        }
        if (state == SOUND_PLAYING) {
            setState(SOUND_STOPPED);
        }
    }

    public void resume() {
        if (player != null && state == SOUND_STOPPED) {
            try {
                player.start();
                setState(SOUND_PLAYING);
            } catch (Throwable e) {
            }
        }
    }

    public void release() {
        closePlayer();
        setState(SOUND_UNINITIALIZED);
    }

    private void closePlayer() {
        Player p = player;
        player = null;
        if (p != null) {
            try {
                p.close();
            } catch (Throwable e) {
            }
        }
    }

    public int getState() {
        return state;
    }

    public int getGain() {
        return gain;
    }

    public void setGain(int gain) {
        if (gain < 0) {
            gain = 0;
        }
        if (gain > 255) {
            gain = 255;
        }
        this.gain = gain;
        applyGain();
    }

    private void applyGain() {
        if (player == null) {
            return;
        }
        try {
            VolumeControl vc = (VolumeControl) player.getControl("VolumeControl");
            if (vc != null) {
                // Nokia gain is 0..255
                vc.setLevel(gain * 100 / 255);
            }
        } catch (Throwable e) {
        }
    }

    public void setSoundListener(SoundListener listener) {
        this.listener = listener;
    }

    private void setState(int newState) {
        state = newState;
        SoundListener l = listener;
        if (l != null) {
            l.soundStateChanged(this, newState);
        }
    }
}

/** Builds a single-track MIDI file of notes for a square-wave voice. */
class MidiWriter {
    private static final int DIVISION = 480;   // ticks per quarter note at 120 bpm
    private final ByteArrayOutputStream track = new ByteArrayOutputStream();
    private int pendingDelta;

    MidiWriter() {
        // Tempo 120 bpm, then General MIDI 81 (square lead): a phone beeper
        meta(0x51, new byte[] { 0x07, (byte) 0xA1, 0x20 });
        event(0, new byte[] { (byte) 0xC0, 80 });
    }

    /** Adds a note (or a rest when midiNote < 0) lasting ms milliseconds. */
    void note(int midiNote, int ms, double gate, int velocity) {
        int ticks = ms * DIVISION / 500;   // 500 ms per quarter at 120 bpm
        if (midiNote < 0 || ticks <= 0) {
            pendingDelta += ticks;
            return;
        }
        int on = (int) (ticks * gate);
        if (on < 1) {
            on = 1;
        }
        event(pendingDelta, new byte[] { (byte) 0x90, (byte) midiNote, (byte) velocity });
        event(on, new byte[] { (byte) 0x80, (byte) midiNote, 0 });
        pendingDelta = ticks - on;
    }

    private void meta(int type, byte[] data) {
        vlq(0);
        track.write(0xFF);
        track.write(type);
        vlq(data.length);
        track.write(data, 0, data.length);
    }

    private void event(int delta, byte[] data) {
        vlq(delta);
        track.write(data, 0, data.length);
    }

    private void vlq(int v) {
        int buffer = v & 0x7F;
        while ((v >>= 7) > 0) {
            buffer <<= 8;
            buffer |= (v & 0x7F) | 0x80;
        }
        for (;;) {
            track.write(buffer & 0xFF);
            if ((buffer & 0x80) != 0) {
                buffer >>= 8;
            } else {
                break;
            }
        }
    }

    byte[] toByteArray() {
        // End of track after any trailing rest
        vlq(pendingDelta);
        track.write(0xFF);
        track.write(0x2F);
        track.write(0);
        byte[] t = track.toByteArray();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] header = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1,
                          (byte) (DIVISION >> 8), (byte) DIVISION,
                          'M', 'T', 'r', 'k' };
        out.write(header, 0, header.length);
        out.write(t.length >>> 24);
        out.write(t.length >>> 16);
        out.write(t.length >>> 8);
        out.write(t.length);
        out.write(t, 0, t.length);
        return out.toByteArray();
    }
}

/**
 * Nokia Smart Messaging ringtone (the "OTA" bit stream games use for
 * FORMAT_TONE) to MIDI.
 */
class RingtoneConverter {
    private static final int[] TEMPOS = { 25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90,
        100, 112, 125, 140, 160, 180, 200, 225, 250, 285, 320, 355, 400, 450, 500, 565,
        635, 715, 800, 900 };

    private final byte[] data;
    private int bit;

    private RingtoneConverter(byte[] data) {
        this.data = data;
    }

    static byte[] toMidi(byte[] data) {
        try {
            return new RingtoneConverter(data).convert();
        } catch (Throwable e) {
            return null;
        }
    }

    private int get(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) {
            if (bit >= data.length * 8) {
                throw new IndexOutOfBoundsException();
            }
            v = (v << 1) | ((data[bit >> 3] >> (7 - (bit & 7))) & 1);
            bit++;
        }
        return v;
    }

    private byte[] convert() {
        int commands = get(8);
        for (int c = 0; c < commands; c++) {
            int command = get(7);
            if (command == 0x25 || command == 0x22) {
                get(1);                       // ringing-tone-programming, unicode
            } else if (command == 0x1D) {
                return song();                // sound: the song follows
            } else {
                return null;
            }
        }
        return null;
    }

    private byte[] song() {
        int type = get(3);
        if (type == 1) {                      // basic song: skip the title
            int titleLength = get(4);
            for (int i = 0; i < titleLength; i++) {
                get(8);
            }
        } else if (type != 2) {               // temporary song has no title
            return null;
        }
        MidiWriter w = new MidiWriter();
        int[][][] patterns = new int[4][][];
        int scale = 1, style = 0, volume = 7, bpm = 63;
        int notes = 0;
        double[] durationSpecifier = { 1.0, 1.5, 1.75, 2.0 / 3.0 };
        int sequenceLength = get(8);
        for (int s = 0; s < sequenceLength; s++) {
            if (get(3) != 0) {                // pattern header id
                return null;
            }
            int id = get(2);
            int loop = get(4);
            int count = get(8);
            int[][] instructions;
            if (count == 0 && patterns[id] != null) {
                instructions = patterns[id];  // repeat of an earlier pattern
            } else {
                instructions = new int[count][];
                for (int i = 0; i < count; i++) {
                    int kind = get(3);
                    switch (kind) {
                    case 1: instructions[i] = new int[] { 1, get(4), get(3), get(2) }; break;
                    case 2: case 3: instructions[i] = new int[] { kind, get(2) }; break;
                    case 4: instructions[i] = new int[] { 4, get(5) }; break;
                    case 5: instructions[i] = new int[] { 5, get(4) }; break;
                    default: return null;
                    }
                }
                patterns[id] = instructions;
            }
            // Loop value 15 is "forever"; Sound.play's loop count covers that
            int repeats = 1 + (loop == 15 ? 0 : loop);
            for (int r = 0; r < repeats; r++) {
                for (int i = 0; i < instructions.length; i++) {
                    int[] in = instructions[i];
                    switch (in[0]) {
                    case 1: {
                        // Duration: a full note (4 beats) halved per step,
                        // then dotted, double dotted or triplet
                        double beats = 4.0 / (1 << Math.min(in[2], 5));
                        int ms = (int) (beats * durationSpecifier[in[3]] * 60000 / bpm);
                        double gate = style == 1 ? 1.0 : style == 2 ? 0.5 : 0.9;
                        // Notes 1-12 are C..B of the current scale; A of
                        // scale 0 is 440 Hz, so its C is MIDI note 60
                        int note = in[1] >= 1 && in[1] <= 12 ? 60 + 12 * scale + in[1] - 1 : -1;
                        w.note(note, ms, gate, 40 + volume * 87 / 15);
                        notes++;
                        break;
                    }
                    case 2: scale = in[1]; break;
                    case 3: style = in[1]; break;
                    case 4: bpm = TEMPOS[in[1]]; break;
                    case 5: volume = in[1]; break;
                    }
                }
            }
        }
        return notes > 0 ? w.toByteArray() : null;
    }
}
