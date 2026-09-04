package javax.microedition.m3g;

public class Camera extends Node {
    public static final int GENERIC = 48;
    public static final int PARALLEL = 49;
    public static final int PERSPECTIVE = 50;

    private int projType = PERSPECTIVE;
    private float fovy = 60f, aspect = 1f, near = 1f, far = 1000f;

    private static int diagSetPerspCount = 0;

    public void setPerspective(float fovy, float aspectRatio, float near, float far) {
        if (diagSetPerspCount < 20) {
            diagSetPerspCount++;
            System.out.println("M3G_SET_PERSPECTIVE fovy=" + fovy + " aspect=" + aspectRatio
                    + " near=" + near + " far=" + far);
        }
        this.projType = PERSPECTIVE;
        this.fovy = fovy;
        this.aspect = aspectRatio;
        this.near = near;
        this.far = far;
    }

    private static int diagGetProjCount = 0;

    /** Fills `transform` with the current projection matrix; returns the projection type. */
    public int getProjection(Transform transform) {
        /*
         * This port's native Math.tan()/Math.cos()/Math.sin() (the whole
         * jvm_fplib_* fdlibm family) return garbage for perfectly ordinary
         * angles here - confirmed via diagnostic logging: fovy=45/49.5deg
         * gave Math.tan()-based cot(fovy/2) = -0.0, and even Math.cos()/
         * Math.sin() directly still gave ~1e-23. See Transform.fastSin/
         * fastCos for the full story and the pure-Java replacement used
         * instead (also fixes Transform.postRotate(), which was silently
         * producing degenerate rotation matrices from the same bug).
         */
        double half = Math.toRadians(fovy) / 2.0;
        float f = (float) (Transform.fastCos(half) / Transform.fastSin(half));
        if (diagGetProjCount < 20) {
            diagGetProjCount++;
            System.out.println("M3G_GET_PROJECTION fovy=" + fovy + " aspect=" + aspect
                    + " near=" + near + " far=" + far + " f=" + f);
        }
        float[] p = new float[16];
        p[0] = f / aspect; p[1] = 0; p[2] = 0;                              p[3] = 0;
        p[4] = 0;          p[5] = f; p[6] = 0;                              p[7] = 0;
        p[8] = 0;          p[9] = 0; p[10] = (far + near) / (near - far);   p[11] = (2 * far * near) / (near - far);
        p[12] = 0;         p[13] = 0; p[14] = -1;                          p[15] = 0;
        transform.set(p);
        return projType;
    }

    float getFovy() { return fovy; }
    float getAspect() { return aspect; }
    float getNear() { return near; }
    float getFar() { return far; }
}
