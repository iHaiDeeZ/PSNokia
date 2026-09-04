/*
 * Minimal JSR 184 (M3G) implementation for phoneME/Vita, scoped to what
 * real bundled MIDlets (Tower Bloxx) actually use: static (non-animated,
 * unlit) scene graphs loaded from a .m3g file, rendered via a native
 * software rasterizer. Animation, skinning, morphing, lighting, fog and
 * sprites are intentionally not modeled - see M3G build notes.
 */
package javax.microedition.m3g;

public abstract class Object3D {
    int userID;

    public int getUserID() {
        return userID;
    }

    public void setUserID(int id) {
        userID = id;
    }
}
