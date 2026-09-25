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

import javax.microedition.media.control.MIDIControl;

/**
 * The "device://midi" live MIDI device Player. Real bundled MIDlets that
 * use this (e.g. MeBoy, a GB/GBC emulator, driving its own APU emulation
 * through raw note on/off events) only need real-time shortMidiEvent()
 * note synthesis, not actual instrument samples - this backs each MIDI
 * channel's note-on/note-off with the same square-wave tone synthesis
 * Manager.playTone() uses (media_vita.c's CreateToneChunk), not a real
 * GM synth. Program/bank/instrument selection is accepted but ignored -
 * every channel just plays a plain tone at the requested note/velocity.
 * See GenericPlayer's class doc for why a real GM synth (Timidity) isn't
 * built into this port at all - it needs a bundled instrument patch set,
 * a real separate asset dependency this port doesn't have.
 */
public class MIDIPlayer extends ABBBasicPlayer implements MIDIControl {
    private native int nMidiInit();
    private native void nMidiShortEvent(int id, int type, int data1, int data2);
    private native void nMidiClose(int id);

    private int midiId;

    public MIDIPlayer() throws Exception {
        super();
        this.midiId = 0;
    }

    public int getAudioType() {
        return AUDIO_MIDI;
    }

    protected void doRealize() throws MediaException {
    }

    protected void doPrefetch() throws MediaException {
        midiId = nMidiInit();
        if (midiId == 0) { throw new MediaException("Could not open MIDI device"); }
    }

    protected boolean doStart() {
        return true;
    }

    protected void doStop() throws MediaException {
    }

    protected void doDeallocate() {
    }

    protected void doClose() {
        if (midiId != 0) {
            nMidiClose(midiId);
            midiId = 0;
        }
    }

    protected long doSetMediaTime(long now) throws MediaException {
        return TIME_UNKNOWN;
    }

    protected long doGetMediaTime() {
        return TIME_UNKNOWN;
    }

    protected long doGetDuration() {
        return TIME_UNKNOWN;
    }

    protected Control doGetControl(String type) {
        if (type.equals("javax.microedition.media.control.MIDIControl")) { return this; }
        return null;
    }

    public String getContentType() {
        chkClosed(true);
        return "audio/midi";
    }

    /* ---- MIDIControl ---- */

    public int getChannelVolume(int channel) {
        return 100;
    }

    public void setChannelVolume(int channel, int volume) {
    }

    public boolean isBankQuerySupported() {
        return false;
    }

    public int[] getProgram(int channel) throws MediaException {
        return new int[] { 0, 0 };
    }

    public int getBankInstrument(int bank, int prog) throws MediaException {
        return -1;
    }

    public void setProgram(int channel, int bank, int program) {
    }

    public void shortMidiEvent(int type, int data1, int data2) {
        if (midiId == 0) { return; }
        nMidiShortEvent(midiId, type, data1, data2);
    }

    public void longMidiEvent(byte[] data, int offset, int length) {
    }

    public int[] getBankList(boolean custom) throws MediaException {
        return new int[0];
    }

    public String getProgramName(int bank, int prog) throws MediaException {
        return "";
    }

    public int[] getProgramList(int bank) throws MediaException {
        return new int[0];
    }
}
