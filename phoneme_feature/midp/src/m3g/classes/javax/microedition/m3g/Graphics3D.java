package javax.microedition.m3g;

/**
 * Software 3D renderer. Scene-graph walking and matrix composition happen
 * here in Java (infrequent, not performance-critical); the actual
 * per-vertex transform and per-pixel rasterization/texturing happen in
 * native code (nDrawTriangles), since that IS performance-critical and
 * this port's CLDC interpreter is far too slow to do it in Java (see
 * project build notes on interpreted-bytecode per-pixel cost).
 */
public class Graphics3D {
    private static final Graphics3D instance = new Graphics3D();

    private Object boundTarget;
    private final Transform viewMatrix = new Transform();
    private final Transform projMatrix = new Transform();
    private static boolean diagLogged = false;

    private Graphics3D() {
    }

    public static Graphics3D getInstance() {
        return instance;
    }

    public void bindTarget(Object target, boolean depthBuffer, int hints) {
        if (!diagLogged) {
            diagLogged = true;
            System.out.println("M3G_BIND_TARGET");
        }
        boundTarget = target;
        nBindTarget(target, depthBuffer, hints);
    }

    public void releaseTarget() {
        nReleaseTarget();
        boundTarget = null;
    }

    public void clear(Background background) {
        boolean colorClear = background == null || background.isColorClearEnable();
        boolean depthClear = background == null || background.isDepthClearEnable();
        int color = background == null ? 0 : background.getColor();
        nClear(colorClear, color, depthClear);
    }

    public void resetLights() {
        // Lighting is not modeled - real bundled MIDlets render with setMaterial(null) (unlit).
    }

    public void setCamera(Camera camera, Transform transform) {
        viewMatrix.set(transform);
        viewMatrix.invert();
        camera.getProjection(projMatrix);
    }

    private static int renderLogCount = 0;
    private static int smallMeshLogCount = 0;

    /*
     * Tracks every DISTINCT top-level Node ever passed to render() (by
     * identity, not equality - CLDC's Object has no real hashCode()/equals()
     * override machinery worth relying on here), printing once the first
     * time each one is seen. Bounded to a small fixed array so this is safe
     * regardless of how many total render() calls happen (unlike the
     * per-frame-capped `log` flag above, which exhausted its budget on
     * background/menu content alone before any real gameplay was reached).
     * Used to answer, directly, whether the actively-falling/held game
     * piece (rendered via a small per-instance wrapper class in the game's
     * own bytecode - decompiled as `e`/`f`/`g`, each holding one Mesh +
     * Transform, calling straight into this render() method) EVER actually
     * reaches this renderer at all, as opposed to some upstream game-logic
     * condition simply never calling it during whatever's been tested.
     */
    private static final Object[] seenNodes = new Object[150];
    private static int seenNodeCount = 0;

    public void render(Node node, Transform transform) {
        boolean log = renderLogCount < 6;
        if (log) {
            renderLogCount++;
        }
        if (seenNodeCount < seenNodes.length) {
            boolean isNew = true;
            for (int i = 0; i < seenNodeCount; i++) {
                if (seenNodes[i] == node) {
                    isNew = false;
                    break;
                }
            }
            if (isNew) {
                seenNodes[seenNodeCount++] = node;
                String kind = (node instanceof Mesh) ? "Mesh" : (node instanceof Group) ? "Group" : "Node";
                float tx = transform.m[3], ty = transform.m[7], tz = transform.m[11];
                System.out.println("M3G_NEW_TOPLEVEL_NODE #" + seenNodeCount + " kind=" + kind
                        + " transformPos=" + tx + "," + ty + "," + tz);
                if (node instanceof Mesh) {
                    Mesh mesh = (Mesh) node;
                    VertexBuffer vb0 = mesh.getVertexBuffer();
                    if (vb0 != null && vb0.positions != null) {
                        Transform w = new Transform();
                        w.set(transform);
                        w.postMultiply(node.transform);
                        Transform mv2 = new Transform();
                        mv2.set(viewMatrix);
                        mv2.postMultiply(w);
                        float[] mv2f = new float[16];
                        mv2.get(mv2f);
                        float[] pf = new float[16];
                        projMatrix.get(pf);
                        float[] pos = vb0.positions.data;
                        float minX = Float.MAX_VALUE, maxX = -Float.MAX_VALUE;
                        float minY = Float.MAX_VALUE, maxY = -Float.MAX_VALUE;
                        int behind = 0, vcount = pos.length / 3;
                        for (int i = 0; i < vcount; i++) {
                            float vx = pos[i*3], vy = pos[i*3+1], vz = pos[i*3+2];
                            float ex = mv2f[0]*vx+mv2f[1]*vy+mv2f[2]*vz+mv2f[3];
                            float ey = mv2f[4]*vx+mv2f[5]*vy+mv2f[6]*vz+mv2f[7];
                            float ez = mv2f[8]*vx+mv2f[9]*vy+mv2f[10]*vz+mv2f[11];
                            float cx = pf[0]*ex+pf[1]*ey+pf[2]*ez+pf[3];
                            float cy = pf[4]*ex+pf[5]*ey+pf[6]*ez+pf[7];
                            float cw = pf[12]*ex+pf[13]*ey+pf[14]*ez+pf[15];
                            if (cw <= 0) { behind++; continue; }
                            float nx = cx/cw, ny = cy/cw;
                            if (nx < minX) minX = nx; if (nx > maxX) maxX = nx;
                            if (ny < minY) minY = ny; if (ny > maxY) maxY = ny;
                        }
                        System.out.println("M3G_NEW_TOPLEVEL_NDC verts=" + vcount + " behind=" + behind
                                + "/" + vcount + " minX=" + minX + " maxX=" + maxX + " minY=" + minY + " maxY=" + maxY
                                + " eyePos=" + mv2f[3] + "," + mv2f[7] + "," + mv2f[11]);
                    }
                }
            }
        }
        Transform world = new Transform();
        world.set(transform);
        renderNode(node, world);
        if (log) {
            System.out.println("M3G_RENDER_EXIT");
        }
    }

    private void renderNode(Node node, Transform parentWorld) {
        Transform world = new Transform();
        world.set(parentWorld);
        world.postMultiply(node.transform);

        if (node instanceof Mesh) {
            drawMesh((Mesh) node, world);
        } else if (node instanceof Group) {
            Group g = (Group) node;
            int n = g.getChildCount();
            for (int i = 0; i < n; i++) {
                renderNode(g.getChild(i), world);
            }
        }
    }

    private void drawMesh(Mesh mesh, Transform world) {
        VertexBuffer vb = mesh.getVertexBuffer();
        if (vb == null || vb.positions == null) {
            return;
        }
        float[] positions = vb.positions.data;
        float[] texCoords = vb.texCoords != null ? vb.texCoords.data : null;
        float[] modelView = new float[16];
        Transform mv = new Transform();
        mv.set(viewMatrix);
        mv.postMultiply(world);
        mv.get(modelView);
        float[] proj = new float[16];
        projMatrix.get(proj);

        int n = mesh.getSubmeshCount();
        /*
         * Was "<= 6" (an off-by-one against render()'s own "< 6" cap that
         * increments renderLogCount): once renderLogCount reached 6 and
         * stopped incrementing, this stayed permanently true instead of
         * permanently false, so every mesh/submesh/frame printed for real
         * via System.out - a genuine blocking sceIoOpen/Write/Close per
         * print (Java_com_sun_cldchi_io_ConsoleOutputStream_write). Never
         * hit hard until this same session's TriangleStripArray fix made
         * this path actually run with real per-frame triangle data instead
         * of exiting early on an empty index array - tanked FPS from
         * normal to 2 once meshes started actually drawing multiple
         * submeshes every frame. Same "unthrottled per-frame print freezes
         * Vita3K" class of bug documented elsewhere in this project.
         */
        boolean log = renderLogCount < 6;
        if (log) System.out.println("M3G_DRAW_MESH submeshes=" + n + " verts=" + (positions == null ? -1 : positions.length / 3));
        if (log) {
            /*
             * Compact per-mesh on-screen bounding box (NDC, i.e. [-1,1] means
             * on-screen) instead of a full data dump, so many meshes can be
             * sampled across a couple of frames without flooding the log -
             * used to find which mesh is the actively-falling/dropped block
             * (still invisible even after the trig and Appearance fixes)
             * versus background/decoration meshes that may legitimately sit
             * off-screen.
             */
            float minX = Float.MAX_VALUE, maxX = -Float.MAX_VALUE;
            float minY = Float.MAX_VALUE, maxY = -Float.MAX_VALUE;
            int behindCam = 0;
            int vcount = positions.length / 3;
            for (int i = 0; i < vcount; i++) {
                float vx = positions[i * 3], vy = positions[i * 3 + 1], vz = positions[i * 3 + 2];
                float ex = modelView[0]*vx + modelView[1]*vy + modelView[2]*vz + modelView[3];
                float ey = modelView[4]*vx + modelView[5]*vy + modelView[6]*vz + modelView[7];
                float ez = modelView[8]*vx + modelView[9]*vy + modelView[10]*vz + modelView[11];
                float cx = proj[0]*ex + proj[1]*ey + proj[2]*ez + proj[3];
                float cy = proj[4]*ex + proj[5]*ey + proj[6]*ez + proj[7];
                float cw = proj[12]*ex + proj[13]*ey + proj[14]*ez + proj[15];
                if (cw <= 0) { behindCam++; continue; }
                float ndcX = cx / cw, ndcY = cy / cw;
                if (ndcX < minX) minX = ndcX; if (ndcX > maxX) maxX = ndcX;
                if (ndcY < minY) minY = ndcY; if (ndcY > maxY) maxY = ndcY;
            }
            System.out.println("M3G_MESH_NDC minX=" + minX + " maxX=" + maxX + " minY=" + minY
                    + " maxY=" + maxY + " behindCam=" + behindCam + "/" + vcount
                    + " worldPos=" + modelView[3] + "," + modelView[7] + "," + modelView[11]);
        }
        /*
         * Separate, narrowly-targeted diagnostic: the 6-frame-capped `log`
         * above only ever samples whichever meshes happen to render first
         * each frame - in practice that's been the large background city
         * structures every time, never catching the falling block itself.
         * A generic reusable "block" template mesh should be small/unit-
         * scale in its OWN local coordinates (its Transform does the actual
         * per-drop positioning/scaling), unlike the background meshes whose
         * raw vertex data is already in large world-scale units. This scans
         * every frame (cheap - just three abs() checks) but only prints for
         * that specific small-scale signature, with its own low, independent
         * cap - so it can find the block across many frames without
         * reintroducing the per-frame-print FPS regression.
         */
        if (smallMeshLogCount < 20 && positions.length >= 3
                && Math.abs(positions[0]) < 20 && Math.abs(positions[1]) < 20 && Math.abs(positions[2]) < 20) {
            smallMeshLogCount++;
            float minX2 = Float.MAX_VALUE, maxX2 = -Float.MAX_VALUE;
            float minY2 = Float.MAX_VALUE, maxY2 = -Float.MAX_VALUE;
            int behindCam2 = 0;
            int vcount2 = positions.length / 3;
            for (int i = 0; i < vcount2; i++) {
                float vx = positions[i * 3], vy = positions[i * 3 + 1], vz = positions[i * 3 + 2];
                float ex = modelView[0]*vx + modelView[1]*vy + modelView[2]*vz + modelView[3];
                float ey = modelView[4]*vx + modelView[5]*vy + modelView[6]*vz + modelView[7];
                float ez = modelView[8]*vx + modelView[9]*vy + modelView[10]*vz + modelView[11];
                float cx = proj[0]*ex + proj[1]*ey + proj[2]*ez + proj[3];
                float cy = proj[4]*ex + proj[5]*ey + proj[6]*ez + proj[7];
                float cw = proj[12]*ex + proj[13]*ey + proj[14]*ez + proj[15];
                if (cw <= 0) { behindCam2++; continue; }
                float ndcX = cx / cw, ndcY = cy / cw;
                if (ndcX < minX2) minX2 = ndcX; if (ndcX > maxX2) maxX2 = ndcX;
                if (ndcY < minY2) minY2 = ndcY; if (ndcY > maxY2) maxY2 = ndcY;
            }
            int subN = mesh.getSubmeshCount();
            int trisPresent = 0;
            for (int i = 0; i < subN; i++) {
                if (mesh.submeshTriangles[i] != null && mesh.submeshTriangles[i].length > 0) trisPresent++;
            }
            Appearance a0 = subN > 0 ? mesh.getAppearance(0) : null;
            Texture2D t0 = a0 != null ? a0.getTexture(0) : null;
            System.out.println("M3G_SMALLMESH verts=" + vcount2 + " submeshes=" + subN + " trisPresent=" + trisPresent
                    + " minX=" + minX2 + " maxX=" + maxX2 + " minY=" + minY2 + " maxY=" + maxY2
                    + " behindCam=" + behindCam2 + " worldPos=" + modelView[3] + "," + modelView[7] + "," + modelView[11]
                    + " hasTex=" + (t0 != null) + " defColor=" + Integer.toHexString(vb.defaultColor));
        }
        for (int i = 0; i < n; i++) {
            int[] tri = mesh.submeshTriangles[i];
            if (tri == null || tri.length == 0) {
                continue;
            }
            Appearance a = mesh.getAppearance(i);
            Texture2D tex = a != null ? a.getTexture(0) : null;
            int[] texPixels = null;
            int texW = 0, texH = 0, wrapMode = Texture2D.WRAP_REPEAT;
            if (tex != null && tex.getImage() != null) {
                texPixels = tex.getImage().pixels;
                texW = tex.getImage().width;
                texH = tex.getImage().height;
                wrapMode = tex.wrapS;
            }
            int defaultColor = vb.defaultColor;
            if (log) System.out.println("M3G_DRAW_TRIS_BEFORE i=" + i + " triCount=" + tri.length / 3);
            nDrawTriangles(positions, texCoords, tri, texPixels, texW, texH, wrapMode, modelView, proj, defaultColor);
            if (log) System.out.println("M3G_DRAW_TRIS_AFTER i=" + i);
        }
    }

    private static native void nBindTarget(Object target, boolean depthBuffer, int hints);
    private static native void nReleaseTarget();
    private static native void nClear(boolean colorClear, int colorRGB, boolean depthClear);
    private static native void nDrawTriangles(float[] positions, float[] texCoords, int[] triangleIndices,
            int[] texturePixels, int texWidth, int texHeight, int wrapMode,
            float[] modelViewMatrix16, float[] projMatrix16, int defaultColorARGB);
}
