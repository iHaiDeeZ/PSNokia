/*
 * Concrete DirectGraphics implementation backed by a standard
 * javax.microedition.lcdui.Graphics instance. Pixel-drawing operations
 * (drawPixels/drawPolygon/fillPolygon/drawTriangle) are implemented for
 * real via Graphics.drawRGB()/fillTriangle()/drawLine(); drawImage's
 * flip/rotate manipulation is implemented for real too (getRGB() + manual
 * per-pixel rotate/flip + createRGBImage()), cached per (image identity,
 * manipulation) pair - see getTransformedImage()'s doc for why the cache
 * matters. getPixels() reads back from the Graphics' target image.
 */

package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import com.sun.midp.lcdui.GameMap;

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
        int[] out = transform(src, w, h, manipulation);
        int rotation = manipulation & ~(DirectGraphics.FLIP_HORIZONTAL | DirectGraphics.FLIP_VERTICAL);
        boolean swap = rotation == DirectGraphics.ROTATE_90 || rotation == DirectGraphics.ROTATE_270;
        return Image.createRGBImage(out, swap ? h : w, swap ? w : h, true);
    }

    /** Applies a manipulation to w x h ARGB pixels; 90 and 270 swap the sides. */
    private static int[] transform(int[] src, int w, int h, int manipulation) {
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

        return cur;
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
        if (width <= 0 || height <= 0) {
            return;
        }
        int[] argb = new int[width * height];
        boolean alpha = transparency && format == DirectGraphics.TYPE_INT_8888_ARGB;
        for (int row = 0; row < height; row++) {
            int src = offset + row * scanlength;
            int dst = row * width;
            for (int col = 0; col < width; col++) {
                int p = pixels[src + col];
                argb[dst + col] = alpha ? p : (p | 0xFF000000);
            }
        }
        drawARGB(argb, width, height, x, y, manipulation, alpha);
    }

    public void drawPixels(short[] pixels, boolean transparency, int offset, int scanlength,
            int x, int y, int width, int height, int manipulation, int format) {
        diagLog("drawPixels[short]", width * height);
        if (width <= 0 || height <= 0) {
            return;
        }
        int[] argb = new int[width * height];
        for (int row = 0; row < height; row++) {
            int src = offset + row * scanlength;
            int dst = row * width;
            for (int col = 0; col < width; col++) {
                int p = shortToARGB(pixels[src + col], format);
                argb[dst + col] = transparency ? p : (p | 0xFF000000);
            }
        }
        drawARGB(argb, width, height, x, y, manipulation, transparency);
    }

    public void drawPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
            int x, int y, int width, int height, int manipulation, int format) {
        diagLog("drawPixels[byte]", width * height);
        if (width <= 0 || height <= 0) {
            return;
        }
        int[] argb = new int[width * height];
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                int a = 0xFF;
                int rgb;
                if (format == DirectGraphics.TYPE_BYTE_1_GRAY
                        || format == DirectGraphics.TYPE_BYTE_1_GRAY_VERTICAL) {
                    // One bit per pixel, most significant first; a set bit is
                    // black. The mask has the same layout; a set bit is opaque.
                    int byteIndex, bit;
                    if (format == DirectGraphics.TYPE_BYTE_1_GRAY) {
                        int b = offset * 8 + row * scanlength + col;
                        byteIndex = b >> 3;
                        bit = 7 - (b & 7);
                    } else {
                        // Vertical: each byte holds 8 pixels of a column
                        int b = offset * 8 + row;
                        byteIndex = (b >> 3) * scanlength + col;
                        bit = b & 7;
                    }
                    rgb = ((pixels[byteIndex] >> bit) & 1) != 0 ? 0x000000 : 0xFFFFFF;
                    if (transparencyMask != null) {
                        a = ((transparencyMask[byteIndex] >> bit) & 1) != 0 ? 0xFF : 0x00;
                    }
                } else {
                    int idx = offset + row * scanlength + col;
                    rgb = byteToRGB(pixels[idx], format);
                    if (transparencyMask != null) {
                        a = transparencyMask[idx] != 0 ? 0xFF : 0x00;
                    }
                }
                argb[row * width + col] = (a << 24) | rgb;
            }
        }
        drawARGB(argb, width, height, x, y, manipulation, transparencyMask != null);
    }

    /** Draws w x h ARGB pixels at (x, y) after a flip/rotate manipulation. */
    private void drawARGB(int[] argb, int w, int h, int x, int y, int manipulation,
            boolean alpha) {
        int dw = w, dh = h;
        if (manipulation != 0) {
            argb = transform(argb, w, h, manipulation);
            int rotation = manipulation & ~(DirectGraphics.FLIP_HORIZONTAL | DirectGraphics.FLIP_VERTICAL);
            if (rotation == DirectGraphics.ROTATE_90 || rotation == DirectGraphics.ROTATE_270) {
                dw = h;
                dh = w;
            }
        }
        g.drawRGB(argb, 0, dw, x, y, dw, dh, alpha);
    }

    private static int shortToARGB(short px, int format) {
        int v = px & 0xFFFF;
        switch (format) {
            case DirectGraphics.TYPE_USHORT_4444_ARGB: {
                int a = (v >> 12) & 0xF, r = (v >> 8) & 0xF, gr = (v >> 4) & 0xF, b = v & 0xF;
                return ((a * 17) << 24) | ((r * 17) << 16) | ((gr * 17) << 8) | (b * 17);
            }
            case DirectGraphics.TYPE_USHORT_444_RGB: {
                int r = (v >> 8) & 0xF, gr = (v >> 4) & 0xF, b = v & 0xF;
                return 0xFF000000 | ((r * 17) << 16) | ((gr * 17) << 8) | (b * 17);
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

    private static short argbToShort(int p, int format) {
        int a = (p >>> 24) & 0xFF, r = (p >> 16) & 0xFF, gr = (p >> 8) & 0xFF, b = p & 0xFF;
        switch (format) {
            case DirectGraphics.TYPE_USHORT_4444_ARGB:
                return (short) (((a >> 4) << 12) | ((r >> 4) << 8) | ((gr >> 4) << 4) | (b >> 4));
            case DirectGraphics.TYPE_USHORT_444_RGB:
                return (short) (((r >> 4) << 8) | ((gr >> 4) << 4) | (b >> 4));
            case DirectGraphics.TYPE_USHORT_1555_ARGB:
                return (short) ((a >= 0x80 ? 0x8000 : 0) | ((r >> 3) << 10) | ((gr >> 3) << 5) | (b >> 3));
            case DirectGraphics.TYPE_USHORT_555_RGB:
                return (short) (((r >> 3) << 10) | ((gr >> 3) << 5) | (b >> 3));
            case DirectGraphics.TYPE_USHORT_565_RGB:
            default:
                return (short) (((r >> 3) << 11) | ((gr >> 2) << 5) | (b >> 3));
        }
    }

    private static int byteToRGB(byte px, int format) {
        int v = px & 0xFF;
        switch (format) {
            case DirectGraphics.TYPE_BYTE_332_RGB: {
                int r = (v >> 5) & 0x7, gr = (v >> 2) & 0x7, b = v & 0x3;
                return ((r * 255 / 7) << 16) | ((gr * 255 / 7) << 8) | (b * 85);
            }
            case DirectGraphics.TYPE_BYTE_4_GRAY:
                v = (v & 0xF) * 17;
                return (v << 16) | (v << 8) | v;
            case DirectGraphics.TYPE_BYTE_2_GRAY:
                v = (v & 0x3) * 85;
                return (v << 16) | (v << 8) | v;
            case DirectGraphics.TYPE_BYTE_8_GRAY:
            default:
                return (v << 16) | (v << 8) | v;
        }
    }

    /**
     * Reads w x h ARGB pixels at (x, y) of this Graphics' target image into
     * argb. Returns false when the target cannot be read (the screen).
     */
    private boolean readARGB(int[] argb, int x, int y, int w, int h) {
        Image target = GameMap.getGraphicsAccess() == null ? null
                : GameMap.getGraphicsAccess().getGraphicsImage(g);
        if (target == null || w <= 0 || h <= 0) {
            return false;
        }
        x += g.getTranslateX();
        y += g.getTranslateY();
        // Clip the request to the image; pixels outside it stay transparent
        int x0 = Math.max(x, 0), y0 = Math.max(y, 0);
        int x1 = Math.min(x + w, target.getWidth()), y1 = Math.min(y + h, target.getHeight());
        if (x1 <= x0 || y1 <= y0) {
            return true;
        }
        int[] tmp = new int[(x1 - x0) * (y1 - y0)];
        target.getRGB(tmp, 0, x1 - x0, x0, y0, x1 - x0, y1 - y0);
        for (int row = y0; row < y1; row++) {
            System.arraycopy(tmp, (row - y0) * (x1 - x0), argb, (row - y) * w + (x0 - x), x1 - x0);
        }
        return true;
    }

    public void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        int[] argb = new int[Math.max(width * height, 0)];
        if (!readARGB(argb, x, y, width, height)) {
            return;
        }
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                int p = argb[row * width + col];
                int lum = (((p >> 16) & 0xFF) * 3 + ((p >> 8) & 0xFF) * 6 + (p & 0xFF)) / 10;
                boolean opaque = (p >>> 24) >= 0x80;
                if (format == DirectGraphics.TYPE_BYTE_1_GRAY) {
                    int b = offset * 8 + row * scanlength + col;
                    int mask = 1 << (7 - (b & 7));
                    pixels[b >> 3] = (byte) (lum < 128 ? pixels[b >> 3] | mask : pixels[b >> 3] & ~mask);
                    if (transparencyMask != null) {
                        transparencyMask[b >> 3] = (byte) (opaque ? transparencyMask[b >> 3] | mask
                                : transparencyMask[b >> 3] & ~mask);
                    }
                } else {
                    int idx = offset + row * scanlength + col;
                    int r = (p >> 16) & 0xFF, gr = (p >> 8) & 0xFF, b = p & 0xFF;
                    pixels[idx] = (byte) (format == DirectGraphics.TYPE_BYTE_332_RGB
                            ? ((r >> 5) << 5) | ((gr >> 5) << 2) | (b >> 6) : lum);
                    if (transparencyMask != null) {
                        transparencyMask[idx] = (byte) (opaque ? 0xFF : 0);
                    }
                }
            }
        }
    }

    public void getPixels(short[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        int[] argb = new int[Math.max(width * height, 0)];
        if (!readARGB(argb, x, y, width, height)) {
            return;
        }
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                pixels[offset + row * scanlength + col] = argbToShort(argb[row * width + col], format);
            }
        }
    }

    public void getPixels(int[] pixels, int offset, int scanlength,
            int x, int y, int width, int height, int format) {
        int[] argb = new int[Math.max(width * height, 0)];
        if (!readARGB(argb, x, y, width, height)) {
            return;
        }
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                int p = argb[row * width + col];
                pixels[offset + row * scanlength + col] =
                        format == DirectGraphics.TYPE_INT_8888_ARGB ? p : (p & 0xFFFFFF);
            }
        }
    }

    public int getNativePixelFormat() {
        return DirectGraphics.TYPE_USHORT_565_RGB;
    }
}
