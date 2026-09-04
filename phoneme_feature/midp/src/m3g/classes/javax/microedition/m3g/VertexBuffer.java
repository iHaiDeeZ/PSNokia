package javax.microedition.m3g;

public class VertexBuffer extends Object3D {
    VertexArray positions;
    VertexArray texCoords;
    VertexArray colors;
    VertexArray normals;
    int defaultColor = 0xFFFFFFFF;

    public void setPositions(VertexArray v) { positions = v; }
    public void setTexCoords(int unit, VertexArray v, float scaleU, float scaleV, float biasU, float biasV) { texCoords = v; }
    public void setColors(VertexArray v) { colors = v; }
    public void setNormals(VertexArray v) { normals = v; }
    public void setDefaultColor(int argb) { defaultColor = argb; }

    public VertexArray getPositions(float[] scaleBias) { return positions; }
}
