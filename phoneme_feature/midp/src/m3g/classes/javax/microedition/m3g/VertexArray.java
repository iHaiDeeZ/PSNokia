package javax.microedition.m3g;

/**
 * Holds decoded per-vertex float data (positions, texcoords, colors, ...).
 * Real M3G stores these as byte/short with a scale+bias; this port
 * decodes to float once at load time (not performance-critical - it runs
 * once per mesh load, not per frame) so the native rasterizer only ever
 * deals with plain float arrays.
 */
public class VertexArray extends Object3D {
    final int componentCount;   // e.g. 3 for xyz, 2 for uv, 4 for rgba
    final int vertexCount;
    /** vertexCount * componentCount floats, read directly by the native rasterizer. */
    final float[] data;

    public VertexArray(int vertexCount, int componentCount, float[] data) {
        this.vertexCount = vertexCount;
        this.componentCount = componentCount;
        this.data = data;
    }

    public int getVertexCount() { return vertexCount; }
    public int getComponentCount() { return componentCount; }
}
