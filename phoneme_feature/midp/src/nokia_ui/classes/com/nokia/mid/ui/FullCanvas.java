/*
 * Minimal implementation of the Nokia UI API's FullCanvas - a full-screen
 * (no title/softkey-bar) canvas variant real bundled MIDlets subclass
 * directly. Key code constants match the published Nokia UI API 1.1 spec.
 */

package com.nokia.mid.ui;

import javax.microedition.lcdui.game.GameCanvas;

public abstract class FullCanvas extends GameCanvas {

    public static final int KEY_UP_ARROW = -1;
    public static final int KEY_DOWN_ARROW = -2;
    public static final int KEY_LEFT_ARROW = -3;
    public static final int KEY_RIGHT_ARROW = -4;
    public static final int KEY_SOFTKEY1 = -6;
    public static final int KEY_SOFTKEY2 = -7;
    public static final int KEY_SOFTKEY3 = -8;
    public static final int KEY_SEND = -10;
    public static final int KEY_END = -11;

    protected FullCanvas() {
        super(false);
        /*
         * A real FullCanvas is, by definition, full-screen with no title or
         * softkey bar - this was missing here, and it isn't cosmetic: Chameleon's
         * SoftButtonLayer.isSoft1Active()/isSoft2Active() (lfjava/chameleon/
         * layers/SoftButtonLayer.java) unconditionally absorb SOFT1(-6)/SOFT2(-7)
         * key events for any Canvas NOT in full-screen mode, regardless of
         * whether a real Command is bound to that slot - only in full-screen
         * mode does it fall back to checking for an actual active command.
         * Without this call, any FullCanvas subclass that manages its own
         * softkey UI manually (drawing labels itself, dispatching on raw
         * keyPressed(-6)/(-7) instead of the standard addCommand()/
         * CommandListener API - confirmed via bytecode to be exactly what Tower
         * Bloxx's language/audio-dialog screens do) has 100% of its softkey
         * presses silently swallowed by Chameleon before Canvas.keyPressed()
         * is ever called, with zero error/log/exception anywhere - looks
         * identical to "input isn't working" from every angle except that
         * other keys (arrows, numeric, fire) work completely normally.
         */
        setFullScreenMode(true);
    }
}
