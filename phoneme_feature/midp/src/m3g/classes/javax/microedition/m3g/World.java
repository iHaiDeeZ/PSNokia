package javax.microedition.m3g;

import java.util.Hashtable;

public class World extends Group {
    Camera activeCamera;
    Background background;
    /** Every Object3D parsed in the same Loader.load() call as this World, keyed by userID (0 excluded). Set by Loader. */
    Hashtable byUserID;

    public Camera getActiveCamera() {
        return activeCamera;
    }

    public void setActiveCamera(Camera camera) {
        activeCamera = camera;
    }

    public Background getBackground() {
        return background;
    }

    public void setBackground(Background bg) {
        background = bg;
    }

    /**
     * Recursively searches this world's node tree for an Object3D whose
     * userID matches. Not part of the base JSR184 spec, but real bundled
     * MIDlets (Tower Bloxx) call it directly - a documented Nokia-runtime
     * convenience extension, reasonably implemented here as a depth-first
     * search over every parsed object reachable from this world (not just
     * the Node tree, so non-Node objects like Appearance/Material found
     * during the same load are matchable too).
     */
    public Object3D find(int userID) {
        if (byUserID == null) {
            return null;
        }
        return (Object3D) byUserID.get(new Integer(userID));
    }
}
