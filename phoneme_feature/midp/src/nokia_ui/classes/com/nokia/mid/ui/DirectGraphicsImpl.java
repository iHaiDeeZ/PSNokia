/*
 * Concrete DirectGraphics implementation backed by a standard
 * javax.microedition.lcdui.Graphics instance. Pixel-drawing operations
 * (drawPixels/drawPolygon/fillPolygon/drawTriangle) are implemented for
 * real via Graphics.drawRGB()/fillTriangle()/drawLine(); drawImage's
 * flip/rotate manipulation is implemented for real too (getRGB() + manual
 * per-pixel rotate/flip + createRGBImage()), cached per (image identity,
 * manipulation) pair - see getTransformedImage()'s doc for why the cache
 * matters. The getPixels() readback family remains unimplemented (needs
 * native pixel access this port doesn't currently expose).
 */

package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

class DirectGraphicsImpl implements DirectGraphics {

    private Graphics g;
    private int argbColor;

    private static int diagLogCount = 0;
    private static long diagPixelTotal = 0;
    private static void diagLog(String tag, int pixelCount) {
        diagPixelTotal += pixelCount;
        if (diagLogCount < 40) {
            diagLogCount++;
            System.out.println("NOKIAUI_DIAG " + tag + " px=" + pixelCount
                    + " total=" + diagPixelTotal + " t=" + System.currentTimeMillis());
        }
    }

    DirectGraphicsImpl(Graphics g) {
        this.g = g;
        this.argbColor = 0xFF000000 | g.getColor();
    }

    public int getAlphaComponent() {
        return (argbColor >>> 24) & 0xFF;
    }

    public void setARGBColor(int argbColor) {
        this.argbColor = argbColor;
        g.setColor(argbColor & 0x00FFFFFF);
    }

    // Cache of (image identity, manipulation) -> pre-transformed Image. The
    // naive approach (recompute the rotated/flipped pixel buffer on every
    // drawImage call, via getRGB()+manual per-pixel index math+createRGBImage)
    // is a genuine interpreted-bytecode per-pixel loop - tried unthrottled
    // once before and dropped a game's FPS from ~10 to ~2 (a large image
    // redrawn with manipulation every single frame for scrolling background
    // tiles). Caching keyed by (image, manipulation) turns that into a
    // one-time cost per distinct combo - cheap for the common case (static
    // level decoration built from a handful of mirrored/rotated source
    // tiles), same as any other per-mesh-not-per-frame cache in this port.
    private static final int MAX_TRANSFORM_CACHE = 64;
    private static final int[] cacheKey = new int[MAX_TRANSFORM_CACHE];
    private static final Image[] cacheImg = new Image[MAX_TRANSFORM_CACHE];
    private static final long[] cacheAge = new long[MAX_TRANSFORM_CACHE];
    private static int cacheCount = 0;
    private static long cacheClock = 0;

    public void drawImage(Image img, int x, int y, int anchor, int manipulation) {
        diagLog("drawImage", img.getWidth() * img.getHeight());
        if (manipulation == 0) {
            g.drawImage(img, x, y, anchor);
            return;
        }
        g.drawImage(getTransformedImage(img, manipulation), x, y, anchor);
    }

    private static Image getTransformedImage(Image img, int manipulation) {
        int key = System.identityHashCode(img) * 31 + manipulation;
        for (int i = 0; i < cacheCount; i++) {
            if (cacheKey[i] == key) {
                cacheAge[i] = ++cacheClock;
                return cacheImg[i];
            }
        }
        Image result = computeTransformedImage(img, manipulation);
        int slot;
        if (cacheCount < MAX_TRANSFORM_CACHE) {
            slot = cacheCount++;
        } else {
            slot = 0;
            for (int i = 1; i < MAX_TRANSFORM_CACHE; i++) {
                if (cacheAge[i] < cacheAge[slot]) { slot = i; }
            }
        }
        cacheKey[slot] = key;
        cacheImg[slot] = result;
        cacheAge[slot] = ++cacheClock;
        return result;
    }

    /** Rotation (counter-clockwise) is applied first, then the vertical and
     *  horizontal flips - the order the Nokia UI API specifies. */
    private static Image computeTransformedImage(Image img, int manipulation) {
        int w = img.getWidth();
        int h = img.getHeight();
        int[] src = new int[w * h];
        img.getRGB(src, 0, w, 0, 0, w, h);

        boolean flipH = (manipulation & DirectGraphics.FLIP_HORIZONTAL) != 0;
        boolean flipV = (manipulation & DirectGraphics.FLIP_VERTICAL) != 0;
        int rotation = manipulation & ~(DirectGraphics.FLIP_HORIZONTAL | DirectGraphics.FLIP_VERTICAL);

        int[] cur = src;
        int cw = w, ch = h;
        // Nokia's rotations are counter-clockwise
        if (rotation == DirectGraphics.ROTATE_270) {
            int[] tmp = new int[w * h];
            for (int yy = 0; yy < h; yy++) {
                for (int xx = 0; xx < w; xx++) {
                    tmp[xx * h + (h - 1 - yy)] = cur[yy * w + xx];
                }
            }
            cur = tmp; cw = h; ch = w;
        } else if (rotation == DirectGraphics.ROTATE_180) {
            int[] tmp = new int[w * h];
            int n = w * h;
            for (int i = 0; i < n; i++) {
                tmp[n - 1 - i] = cur[i];
            }
            cur = tmp;
        } else if (rotation == DirectGraphics.ROTATE_90) {
            int[] tmp = new int[w * h];
            for (int yy = 0; yy < h; yy++) {
                for (int xx = 0; xx < w; xx++) {
                    tmp[(w - 1 - xx) * h + yy] = cur[yy * w + xx];
                }
            }
            cur = tmp; cw = h; ch = w;
        }

        if (flipH) {
            int[] tmp = new int[cw * ch];
            for (int yy = 0; yy < ch; yy++) {
                for (int xx = 0; xx < cw; xx++) {
                    tmp[yy * cw + (cw - 1 - xx)] = cur[yy * cw + xx];
                }
            }
            cur = tmp;
        }
        if (flipV) {
            int[] tmp = new int[cw * ch];
            for (int yy = 0; yy < ch; yy++) {
                for (int xx = 0; xx < cw; xx++) {
                    tmp[(ch - 1 - yy) * cw + xx] = cur[yy * cw + xx];
                }
            }
            cur = tmp;
        }

        return Image.createRGBImage(cur, cw, ch, true);
    }

    public void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int argbColor) {
        int savedColor = g.getColor();
        g.setColor(argbColor & 0x00FFFFFF);
        g.drawLine(x1, y1, x2, y2);
        g.drawLine(x2, y2, x3, y3);
        g.drawLine(x3, y3, x1, y1);
        g.setColor(savedColor);
    }

    public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
        g.fillTriangle(x1, y1, x2, y2, x3, y3);
    }

    public void drawPolygon(int[] xPoints, int xOffset, int[] yPoints, int yOffset, int nPoints, int argbColor) {
        if (nPoints < 2) {
            return;
        }
        int savedColor = g.getColor();
        g.setColor(argbColor & 0x00FFFFFF);
        for (int i = 0; i < nPoints - 1; i++) {
            g.drawLine(xPoints[xOffset + i], yPoints[yOffset + i],
                       xPoints[xOffset + i + 1], yPoints[yOffset + i + 1]);
        }
        g.drawLine(xPoints[xOffset + nPoints - 1], yPoints[yOffset + nPoints - 1],
                   xPoints[xOffset], yPoints[yOffset]);
        g.setColor(savedColor);
    }

    public void fillPolygon(int[] xPoints, int xOffset, int[] yPoints, int yOffset, int nPoints, int argbColor) {
        if (nPoints < 3) {
            return;
        }
        int savedColor = g.getColor();
        g.setColor(argbColor & 0x00FFFFFF);
        // fan triangulation from the first vertex: exact for convex polygons,
        // a reasonable approximation for concave ones.
        int x0 = xPoints[xOffset];
        int y0 = yPoints[yOffset];
        for (int i = 1; i < nPoints - 1; i++) {
            g.fillTriangle(x0, y0,
                           xPoints[xOffset + i], yPoints[yOffset + i],
                           xPoints[xOffset + i + 1], yPoints[yOffset + i + 1]);
        }
        g.setColor(savedColor);
    }

    public void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength,
            int x, int y, int width, int height, int manipulation, int format) {
        diagLog("drawPixels[int]", width * height);
        g.drawRGB(pixels, offset, scanlength, x, y, width, height, transparency);
    }

    public void drawPixels(short[] pixels, boolean transparency, int offset, int scanlength,
            int x, int y, int width, int height, int manipulation, int format) {
        diagLog("drawPixels[short]", width * height);
        int[] argb = new int[width * height];
        for (int row = 0; row < height; row++) {
            int srcRow = offset + row * scanlength;
            int dstRow = row * width;
            for (int col = 0; col < width; col++) {
                argb[dstRow + col] = shortToARGB(pixels[srcRow + col], format);
            }
        }
        g.drawRGB(argb, 0, width, x, y, width, height, transparency);
    }

    public void drawPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
            int x, int y, int width, int height, int manipulation, int format) {
        diagLog("drawPixels[byte]", width * height);
        int[] argb = new int[width * height];
        for (int row = 0; row < height; row++) {
            int srcRow = offset + row * scanlength;
            int dstRow = row * width;
            for (int col = 0; col < width; col++) {
                int idx = srcRow + col;
                int a = (transparencyMask != null && idx < transparencyMask.length
                        && transparencyMask[idx] == 0) ? 0x00 : 0xFF;
                argb[dstRow + col] = byteToARGB(pixels[idx], format, a);
            }
        }
        g.drawRGB(argb, 0, width, x, y, width, height, transparencyMask != null);
    }

    private static int shortToARGB(short px, int format) {
        int v = px & 0xFFFF;
        switch (format) {
            case DirectGraphics.TYPE_USHORT_4444_ARGB: {
                int a = (v >> 12) & 0xF, r = (v >> 8) & 0xF, gr = (v >> 4) & 0xF, b = v & 0xF;
                a |= a << 4; r |= r << 4; gr |= gr << 4; b |= b << 4;
                return (a << 24) | (r << 16) | (gr << 8) | b;
            }
            case DirectGraphics.TYPE_USHORT_1555_ARGB: {
                int a = ((v >> 15) & 0x1) != 0 ? 0xFF : 0x00;
                int r = (v >> 10) & 0x1F, gr = (v >> 5) & 0x1F, b = v & 0x1F;
                r = (r << 3) | (r >> 2); gr = (gr << 3) | (gr >> 2); b = (b << 3) | (b >> 2);
                return (a << 24) | (r << 16) | (gr << 8) | b;
            }
            case DirectGraphics.TYPE_USHORT_555_RGB: {
                int r = (v >> 10) & 0x1F, gr = (v >> 5) & 0x1F, b = v & 0x1F;
                r = (r << 3) | (r >> 2); gr = (gr << 3) | (gr >> 2); b = (b << 3) | (b >> 2);
                return 0xFF000000 | (r << 16) | (gr << 8) | b;
            }
            case DirectGraphics.TYPE_USHORT_565_RGB:
            default: {
                int r = (v >> 11) & 0x1F, gr = (v >> 5) & 0x3F, b = v & 0x1F;
                r = (r << 3) | (r >> 2); gr = (gr << 2) | (gr >> 4); b = (b << 3) | (b >> 2);
                return 0xFF000000 | (r << 16) | (gr << 8) | b;
            }
        }
    }

    private static int byteToARGB(byte px, int format, int alpha) {
        int v = px & 0xFF;
        switch (format) {
            case DirectGraphics.TYPE_BYTE_332_RGB: {
                int r = (v >> 5) & 0x7, gr = (v >> 2) & 0x7, b = v & 0x3;
                r = (r << 5) | (r << 2) | (r >> 1);
                gr = (gr << 5) | (gr << 2) | (gr >> 1);
                b = (b << 6) | (b << 4) | (b << 2) | b;
                return (alpha << 24) | (r << 16) | (gr << 8) | b;
            }
            case DirectGraphics.TYPE_BYTE_8_GRAY:
            default: {
                return (alpha << 24) | (v << 16) | (v << 8) | v;
            }
        }
    }

    public void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        // pixel readback not implemented on this port.
    }

    public void getPixels(short[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        // pixel readback not implemented on this port.
    }

    public void getPixels(int[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        // pixel readback not implemented on this port.
    }

    public int getNativePixelFormat() {
        return DirectGraphics.TYPE_USHORT_565_RGB;
    }
}
