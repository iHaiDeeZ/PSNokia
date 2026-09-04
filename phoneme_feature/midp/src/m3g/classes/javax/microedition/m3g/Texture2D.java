package javax.microedition.m3g;

public class Texture2D extends Object3D {
    public static final int FUNC_ADD = 224;
    public static final int FUNC_BLEND = 225;
    public static final int FUNC_DECAL = 226;
    public static final int FUNC_MODULATE = 227;
    public static final int FUNC_REPLACE = 228;

    public static final int WRAP_CLAMP = 240;
    public static final int WRAP_REPEAT = 241;

    public static final int FILTER_BASE_LEVEL = 208;
    public static final int FILTER_LINEAR = 209;
    public static final int FILTER_NEAREST = 210;

    final Image2D image;
    /** Only FUNC_REPLACE (the only mode real bundled MIDlets use) is actually honored by the rasterizer; others fall back to it. */
    int blending = FUNC_MODULATE;
    int wrapS = WRAP_REPEAT, wrapT = WRAP_REPEAT;

    public Texture2D(Image2D image) {
        this.image = image;
    }

    public void setBlending(int func) { blending = func; }
    public void setWrapping(int wrapS, int wrapT) { this.wrapS = wrapS; this.wrapT = wrapT; }
    public Image2D getImage() { return image; }
}
