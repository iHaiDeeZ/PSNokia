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

import java.io.IOException;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;

import javax.microedition.media.control.VolumeControl;

/**
 * Real WAV sample playback via the native SDL2 mixer (media_vita.c /
 * vita_mix_shim.c) - the whole InputStream is buffered up front (typical
 * J2ME sound-effect WAVs are small; this keeps decoding/streaming simple)
 * and handed to SDL_LoadWAV_RW natively. Other content types (anything
 * that isn't real PCM WAV data SDL2 can parse) fail cleanly at realize()
 * with a MediaException, same as an unsupported type should.
 */
public class GenericPlayer extends ABBBasicPlayer implements VolumeControl {
    private native int nWavLoad(byte[] data);
    private native int nWavStart(int id, int loops);
    private native void nWavStop(int id);
    private native void nWavDeallocate(int id);
    private native void nWavClose(int id);
    private native long nWavGetDuration(int id);
    private native int nWavCheckEOM(int id);
    private native void nWavSetVolume(int id, int level0to100);

    private final String contentType;
    private byte[] data;
    private int wavId;
    private Thread checkThread;
    private int volumeLevel = 100;

    public GenericPlayer(String contentType) {
        super();
        this.contentType = contentType;
        this.wavId = 0;
    }

    public int getAudioType() {
        return AUDIO_PCM;
    }

    public void setSource(InputStream stream) throws IOException, MediaException {
        super.setSource(stream);
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        byte[] tmp = new byte[4096];
        int n;
        while ((n = stream.read(tmp)) >= 0) {
            if (n > 0) { buf.write(tmp, 0, n); }
        }
        this.data = buf.toByteArray();
    }

    protected void doRealize() throws MediaException {
        if (data == null) { throw new MediaException("no source set"); }
        wavId = nWavLoad(data);
        if (wavId == 0) {
            System.out.println("MEDIA: " + contentType + " load FAILED (" + data.length + " bytes)"); throw new MediaException("Unsupported or invalid WAV data for: " + contentType); }
        data = null; // native side now owns a decoded copy
    }

    protected void doPrefetch() throws MediaException {
    }

    protected boolean doStart() {
        int loops = (loopCountSet == -1) ? -1 : (loopCountSet > 1 ? loopCountSet - 1 : 0);
        if (nWavStart(wavId, loops) != 0) { return false; }
        return true;
    }

    protected void doPostStart() {
        checkThread = new Thread(new Runnable() {
            public void run() {
                while (state == Player.STARTED) {
                    if (nWavCheckEOM(wavId) != 0) {
                        sendEvent(PlayerListener.END_OF_MEDIA, new Long(getMediaTime()));
                        return;
                    }
                    Thread.yield();
                }
            }
        });
        checkThread.start();
    }

    protected void doStop() throws MediaException {
        nWavStop(wavId);
    }

    protected void doDeallocate() {
        nWavDeallocate(wavId);
    }

    protected void doClose() {
        if (wavId != 0) {
            nWavClose(wavId);
            wavId = 0;
        }
    }

    protected long doSetMediaTime(long now) throws MediaException {
        return TIME_UNKNOWN;
    }

    protected long doGetMediaTime() {
        return TIME_UNKNOWN;
    }

    protected long doGetDuration() {
        if (wavId == 0) { return TIME_UNKNOWN; }
        return nWavGetDuration(wavId);
    }

    protected Control doGetControl(String type) {
        if (type.equals("javax.microedition.media.control.VolumeControl")) { return this; }
        return null;
    }

    public String getContentType() {
        chkClosed(true);
        return contentType;
    }

    public int setLevel(int level) {
        if (level < 0) { level = 0; }
        if (level > 100) { level = 100; }
        volumeLevel = level;
        if (wavId != 0) { nWavSetVolume(wavId, volumeLevel); }
        sendEvent(PlayerListener.VOLUME_CHANGED, this);
        return volumeLevel;
    }

    public int getLevel() {
        return volumeLevel;
    }

    public boolean isMuted() {
        return volumeLevel == 0;
    }

    public void setMute(boolean mute) {
        setLevel(mute ? 0 : volumeLevel);
    }
}
