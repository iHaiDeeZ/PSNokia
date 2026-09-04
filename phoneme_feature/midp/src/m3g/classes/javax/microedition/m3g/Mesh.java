package javax.microedition.m3g;

public class Mesh extends Node {
    VertexBuffer vertices;
    /** Per submesh: flattened triangle indices (groups of 3) into `vertices`. Strips are expanded to plain triangle lists at load time. */
    int[][] submeshTriangles;
    Appearance[] appearances;

    public Mesh(VertexBuffer vertices, int[][] submeshTriangles, Appearance[] appearances) {
        this.vertices = vertices;
        this.submeshTriangles = submeshTriangles;
        this.appearances = appearances;
    }

    public int getSubmeshCount() {
        return submeshTriangles.length;
    }

    public Appearance getAppearance(int index) {
        return appearances[index];
    }

    public void setAppearance(int index, Appearance a) {
        appearances[index] = a;
    }

    public VertexBuffer getVertexBuffer() {
        return vertices;
    }
}
