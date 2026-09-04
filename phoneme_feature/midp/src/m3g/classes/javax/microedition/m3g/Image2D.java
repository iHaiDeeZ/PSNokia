package javax.microedition.m3g;

/** Texture pixel source. Pixel data is normalized to ARGB int[] regardless of source format for simplicity. */
public class Image2D extends Object3D {
    public static final int ALPHA = 96;
    public static final int LUMINANCE = 97;
    public static final int LUMINANCE_ALPHA = 98;
    public static final int RGB = 99;
    public static final int RGBA = 100;

    final int width, height;
    /** ARGB8888, row-major, width*height entries. Read directly by the native rasterizer. */
    final int[] pixels;

    public Image2D(int format, int width, int height, int[] argbPixels) {
        this.width = width;
        this.height = height;
        this.pixels = argbPixels;
    }

    public int getWidth() { return width; }
    public int getHeight() { return height; }
}
