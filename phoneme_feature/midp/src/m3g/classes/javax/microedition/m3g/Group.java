package javax.microedition.m3g;

import java.util.Vector;

public class Group extends Node {
    final Vector children = new Vector();

    void addChild(Node child) {
        child.parent = this;
        children.addElement(child);
    }

    public int getChildCount() {
        return children.size();
    }

    public Node getChild(int index) {
        return (Node) children.elementAt(index);
    }
}
