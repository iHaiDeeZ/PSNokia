/*
 * Minimal, functional implementation of the Nokia UI API's DirectGraphics
 * interface for phoneME ports that don't otherwise provide com.nokia.mid.ui.
 * Method signatures match the published Nokia UI API 1.1 specification so
 * that MIDlets compiled/preverified against a real Nokia SDK resolve and
 * verify correctly against this classpath.
 */

package com.nokia.mid.ui;

import javax.microedition.lcdui.Image;

public interface DirectGraphics {

    public static final int FLIP_HORIZONTAL = 8192;
    public static final int FLIP_VERTICAL = 16384;
    public static final int ROTATE_90 = 90;
    public static final int ROTATE_180 = 180;
    public static final int ROTATE_270 = 270;

    public static final int TYPE_BYTE_1_GRAY = 1;
    public static final int TYPE_BYTE_1_GRAY_VERTICAL = 2;
    public static final int TYPE_BYTE_2_GRAY = 3;
    public static final int TYPE_BYTE_4_GRAY = 4;
    public static final int TYPE_BYTE_8_GRAY = 5;
    public static final int TYPE_BYTE_332_RGB = 6;
    public static final int TYPE_USHORT_4444_ARGB = 7;
    public static final int TYPE_USHORT_555_RGB = 8;
    public static final int TYPE_USHORT_565_RGB = 9;
    public static final int TYPE_USHORT_1555_ARGB = 10;
    public static final int TYPE_INT_888_RGB = 11;
    public static final int TYPE_INT_8888_ARGB = 12;
    public static final int TYPE_BYTE_1_GRAY_PACKED_MSB = 128;
    public static final int TYPE_BYTE_1_GRAY_PACKED_LSB = 129;
    public static final int TYPE_BYTE_2_GRAY_PACKED_MSB = 130;
    public static final int TYPE_BYTE_2_GRAY_PACKED_LSB = 131;
    public static final int TYPE_BYTE_4_GRAY_PACKED_MSB = 132;
    public static final int TYPE_BYTE_4_GRAY_PACKED_LSB = 133;

    public int getAlphaComponent();

    public void drawImage(Image img, int x, int y, int anchor, int manipulation);

    public void drawPixels(byte[] pixels, byte[] transparencyMask, int offset,
            int scanlength, int x, int y, int width, int height,
            int manipulation, int format);

    public void drawPixels(short[] pixels, boolean transparency, int offset,
            int scanlength, int x, int y, int width, int height,
            int manipulation, int format);

    public void drawPixels(int[] pixels, boolean transparency, int offset,
            int scanlength, int x, int y, int width, int height,
            int manipulation, int format);

    public void drawPolygon(int[] xPoints, int xOffset, int[] yPoints,
            int yOffset, int nPoints, int argbColor);

    public void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
            int argbColor);

    public void fillPolygon(int[] xPoints, int xOffset, int[] yPoints,
            int yOffset, int nPoints, int argbColor);

    public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);

    public void getPixels(byte[] pixels, byte[] transparencyMask, int offset,
            int scanlength, int x, int y, int width, int height, int format);

    public void getPixels(short[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format);

    public void getPixels(int[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format);

    public int getNativePixelFormat();

    public void setARGBColor(int argbColor);
}
