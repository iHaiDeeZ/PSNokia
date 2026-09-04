package javax.microedition.m3g;

public class Background extends Object3D {
    boolean colorClearEnable = true;
    boolean depthClearEnable = true;
    int color = 0x000000;

    public void setColorClearEnable(boolean enable) { colorClearEnable = enable; }
    public boolean isColorClearEnable() { return colorClearEnable; }
    public void setDepthClearEnable(boolean enable) { depthClearEnable = enable; }
    public boolean isDepthClearEnable() { return depthClearEnable; }
    public void setColor(int argb) { color = argb; }
    public int getColor() { return color; }
}
