/*
 * Minimal implementation of the Nokia UI API's DirectUtils factory class.
 * See DirectGraphics.java for context on why this exists.
 */

package com.nokia.mid.ui;

import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class DirectUtils {

    private DirectUtils() {
    }

    public static DirectGraphics getDirectGraphics(Graphics g) {
        return new DirectGraphicsImpl(g);
    }

    public static Image createImage(int width, int height, int ARGBcolor) {
        int alpha = (ARGBcolor >>> 24) & 0xFF;
        if (alpha == 0) {
            // A real, working transparent canvas - see Image.createTransparentImage
            // and gxj_image.c's copy_imageregion/draw_image fix (session 6,
            // 2026-09-04: the native image compositing code wrote pixel
            // color but never updated a destination's OWN alpha channel
            // during a draw, so a freshly-transparent canvas stayed
            // reporting alpha=0 everywhere even after real content was
            // drawn onto it - fixed at the source, not worked around here).
            return Image.createTransparentImage(width, height);
        }
        Image img = Image.createImage(width, height);
        Graphics g = img.getGraphics();
        g.setColor(ARGBcolor & 0x00FFFFFF);
        g.fillRect(0, 0, width, height);
        return img;
    }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) {
        return Image.createImage(imageData, imageOffset, imageLength);
    }

    public static Font getFont(int face, int style, int size) {
        return Font.getFont(face, style, size);
    }
}
