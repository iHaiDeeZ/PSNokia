/*
 * Standard MIDI File player for Manager.createPlayer(stream, "audio/midi"),
 * played by the software synthesizer in midi_synth.c.
 */

package javax.microedition.media;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import javax.microedition.media.control.VolumeControl;

public class MidiFilePlayer extends ABBBasicPlayer implements VolumeControl {
    private native int nLoad(byte[] data);
    private native void nStart(int id, int loops);
    private native void nStop(int id);
    private native void nRewind(int id);
    private native void nClose(int id);
    private native int nCheckEOM(int id);
    private native void nSetVolume(int id, int level0to100);
    private native long nGetDuration(int id);

    private byte[] data;
    private int songId;
    private Thread checkThread;
    private int volumeLevel = 100;
    private boolean muted;

    public int getAudioType() {
        return AUDIO_MIDI;
    }

    public void setSource(InputStream stream) throws IOException, MediaException {
        super.setSource(stream);
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        byte[] tmp = new byte[4096];
        int n;
        while ((n = stream.read(tmp)) >= 0) {
            if (n > 0) {
                buf.write(tmp, 0, n);
            }
        }
        data = buf.toByteArray();
    }

    protected void doRealize() throws MediaException {
        if (data == null) {
            throw new MediaException("no source set");
        }
        songId = nLoad(data);
        if (songId == 0) {
            System.out.println("MEDIA: MIDI load FAILED (" + data.length + " bytes)");
            throw new MediaException("Invalid MIDI data, or audio unavailable");
        }
        data = null; // the native side keeps its own copy
        nSetVolume(songId, muted ? 0 : volumeLevel);
    }

    protected void doPrefetch() throws MediaException {
    }

    protected boolean doStart() {
        // loopCountSet: -1 forever, else the total number of plays
        int loops = (loopCountSet == -1) ? -1 : (loopCountSet > 1 ? loopCountSet - 1 : 0);
        nStart(songId, loops);
        return true;
    }

    protected void doPostStart() {
        checkThread = new Thread(new Runnable() {
            public void run() {
                while (state == Player.STARTED) {
                    if (nCheckEOM(songId) != 0) {
                        sendEvent(PlayerListener.END_OF_MEDIA, new Long(getMediaTime()));
                        return;
                    }
                    try {
                        Thread.sleep(50);
                    } catch (InterruptedException e) {
                        return;
                    }
                }
            }
        });
        checkThread.start();
    }

    protected void doStop() throws MediaException {
        nStop(songId);
    }

    protected void doDeallocate() {
        if (songId != 0) {
            nStop(songId);
        }
    }

    protected void doClose() {
        if (songId != 0) {
            nClose(songId);
            songId = 0;
        }
    }

    protected long doSetMediaTime(long now) throws MediaException {
        // Only rewinding is supported, which is what games use it for
        if (songId != 0 && now <= 0) {
            nRewind(songId);
            return 0;
        }
        return TIME_UNKNOWN;
    }

    protected long doGetMediaTime() {
        return TIME_UNKNOWN;
    }

    protected long doGetDuration() {
        return songId == 0 ? TIME_UNKNOWN : nGetDuration(songId);
    }

    protected Control doGetControl(String type) {
        if (type.equals("javax.microedition.media.control.VolumeControl")) {
            return this;
        }
        return null;
    }

    public String getContentType() {
        chkClosed(true);
        return "audio/midi";
    }

    public int setLevel(int level) {
        if (level < 0) {
            level = 0;
        }
        if (level > 100) {
            level = 100;
        }
        volumeLevel = level;
        if (songId != 0 && !muted) {
            nSetVolume(songId, volumeLevel);
        }
        sendEvent(PlayerListener.VOLUME_CHANGED, this);
        return volumeLevel;
    }

    public int getLevel() {
        return volumeLevel;
    }

    public boolean isMuted() {
        return muted;
    }

    public void setMute(boolean mute) {
        muted = mute;
        if (songId != 0) {
            nSetVolume(songId, muted ? 0 : volumeLevel);
        }
    }
}
