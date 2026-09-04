package javax.microedition.m3g;

public class Appearance extends Object3D {
    Material material;
    final Texture2D[] textures = new Texture2D[2];

    public void setMaterial(Material m) { material = m; }
    public Material getMaterial() { return material; }

    public void setTexture(int unit, Texture2D t) {
        if (unit >= 0 && unit < textures.length) {
            textures[unit] = t;
        }
    }

    public Texture2D getTexture(int unit) {
        if (unit < 0 || unit >= textures.length) {
            return null;
        }
        return textures[unit];
    }
}
