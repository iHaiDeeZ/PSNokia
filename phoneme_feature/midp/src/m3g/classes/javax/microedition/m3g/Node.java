package javax.microedition.m3g;

public class Node extends Object3D {
    /** Local transform relative to the parent node. */
    public final Transform transform = new Transform();
    Node parent;
}
