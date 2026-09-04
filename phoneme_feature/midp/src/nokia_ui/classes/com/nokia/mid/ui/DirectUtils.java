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
        Image img = Image.createImage(width, height);
        Graphics g = img.getGraphics();
        int alpha = (ARGBcolor >>> 24) & 0xFF;
        g.setColor(ARGBcolor & 0x00FFFFFF);
        if (alpha != 0) {
            g.fillRect(0, 0, width, height);
        }
        return img;
    }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) {
        return Image.createImage(imageData, imageOffset, imageLength);
    }

    public static Font getFont(int face, int style, int size) {
        return Font.getFont(face, style, size);
    }
}
