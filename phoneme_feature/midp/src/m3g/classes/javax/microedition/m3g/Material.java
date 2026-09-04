package javax.microedition.m3g;

/** Kept minimal: real bundled MIDlets (Tower Bloxx) call setMaterial(null), i.e. unlit/untextured-shading is not exercised. */
public class Material extends Object3D {
    int ambientColor = 0x333333;
    int diffuseColor = 0xCCCCCCCC;
    int specularColor = 0x000000;
    int emissiveColor = 0x000000;
    float shininess = 0f;

    public void setColor(int target, int argb) {
        switch (target) {
            case 0x1000: ambientColor = argb; break;
            case 0x1001: diffuseColor = argb; break;
            case 0x1002: emissiveColor = argb; break;
            case 0x1003: specularColor = argb; break;
        }
    }
}
