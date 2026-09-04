package javax.microedition.m3g;

/**
 * 4x4 matrix, stored row-major (m[row*4+col]) so that a point is
 * transformed as v' = M * v with v a column vector. This matches the
 * layout real bundled MIDlets build by hand via set(float[]) (translation
 * components land at indices 3, 7, 11 - the last column of each row).
 */
public class Transform {
    float[] m = new float[16];

    public Transform() {
        setIdentity();
    }

    public void setIdentity() {
        for (int i = 0; i < 16; i++) {
            m[i] = 0f;
        }
        m[0] = m[5] = m[10] = m[15] = 1f;
    }

    public void set(Transform t) {
        System.arraycopy(t.m, 0, m, 0, 16);
    }

    public void set(float[] matrix) {
        System.arraycopy(matrix, 0, m, 0, 16);
    }

    public void get(float[] matrix) {
        System.arraycopy(m, 0, matrix, 0, 16);
    }

    /** Post-multiplies by a translation: this = this * T(x,y,z). */
    public void postTranslate(float x, float y, float z) {
        for (int row = 0; row < 4; row++) {
            int r = row * 4;
            m[r + 3] = m[r] * x + m[r + 1] * y + m[r + 2] * z + m[r + 3];
        }
    }

    /**
     * Pure-Java sin/cos (Taylor series with quadrant folding), used instead
     * of Math.sin()/Math.cos()/Math.tan() everywhere in this M3G engine.
     * This port's native trig (jvm_fplib_sin/cos/tan, CLDC's fdlibm-derived
     * software implementation) was found - via live diagnostic logging
     * while chasing why Tower Bloxx's 3D tower never rendered at all - to
     * return near-zero garbage for perfectly ordinary angles (e.g. a 45deg
     * camera FOV's half-angle, ~0.39 radians): Camera.getProjection()'s
     * cot(fovy/2) came out as -0.0 via Math.tan(), and even routing it
     * through Math.cos()/Math.sin() instead still gave ~1e-23. Whatever
     * bug this is (most likely a word-order/endianness assumption inside
     * fdlibm's raw double bit-manipulation macros, similar in spirit to
     * this project's other confirmed byte-order bugs, but not chased down
     * to its exact root cause - deep VM-internal fdlibm debugging is a
     * much bigger undertaking than working around it here), it silently
     * corrupted every rotation this whole 3D engine ever computed via
     * postRotate() too, not just the camera projection - explaining why
     * the modelView matrices logged alongside the broken projection were
     * ALSO degenerate (first two rows collapsed to zero). Accurate to
     * about 1e-6 for any real angle via full-circle range reduction plus
     * folding into [0, PI/2] - far more than enough for 3D graphics.
     */
    private static final double PI = 3.14159265358979323846;

    /** floor(), also part of the suspect fdlibm family - avoided via plain truncation. */
    private static double floorD(double x) {
        long i = (long) x; // truncates toward zero
        return (x < 0 && (double) i != x) ? (double) (i - 1) : (double) i;
    }

    static double fastSin(double x) {
        double twoPi = 2.0 * PI;
        x = x - twoPi * floorD(x / twoPi + 0.5); // reduce to (-PI, PI]
        boolean neg = x < 0;
        if (neg) x = -x;
        if (x > PI / 2.0) x = PI - x; // sin(PI - x) == sin(x), now in [0, PI/2]
        double x2 = x * x;
        double result = x * (1.0 - x2 / 6.0 * (1.0 - x2 / 20.0 * (1.0 - x2 / 42.0 * (1.0 - x2 / 72.0))));
        return neg ? -result : result;
    }

    static double fastCos(double x) {
        double twoPi = 2.0 * PI;
        x = x - twoPi * floorD(x / twoPi + 0.5); // reduce to (-PI, PI]
        if (x < 0) x = -x; // cos is even
        boolean negate = x > PI / 2.0;
        if (negate) x = PI - x; // cos(PI - x) == -cos(x), now in [0, PI/2]
        double x2 = x * x;
        double result = 1.0 - x2 / 2.0 * (1.0 - x2 / 12.0 * (1.0 - x2 / 30.0 * (1.0 - x2 / 56.0)));
        return negate ? -result : result;
    }

    /**
     * Pure-Java sqrt (Newton-Raphson) - same rationale as fastSin/fastCos
     * above: Math.sqrt() (jvm_sqrt, also part of this port's fdlibm family)
     * was never independently confirmed reliable on this port for this use
     * case, and this axis-normalization step is on the same critical path
     * that was silently broken by the sin/cos/tan bug, so it isn't worth
     * the risk of leaving a same-family native call in the middle of the
     * fix. A handful of Newton iterations from a cheap initial guess is
     * more than accurate enough for normalizing a rotation axis.
     */
    private static double fastSqrt(double x) {
        if (x <= 0) return 0;
        double guess = x;
        for (int i = 0; i < 12; i++) {
            guess = 0.5 * (guess + x / guess);
        }
        return guess;
    }

    /** Post-multiplies by a rotation of `angle` degrees about (ax,ay,az): this = this * R. */
    public void postRotate(float angle, float ax, float ay, float az) {
        float len = (float) fastSqrt(ax * ax + ay * ay + az * az);
        if (len < 1e-8f) {
            return;
        }
        ax /= len; ay /= len; az /= len;
        double rad = Math.toRadians(angle);
        float c = (float) fastCos(rad);
        float s = (float) fastSin(rad);
        float t = 1f - c;

        float[] r = new float[16];
        r[0] = t * ax * ax + c;      r[1] = t * ax * ay - s * az; r[2] = t * ax * az + s * ay; r[3] = 0f;
        r[4] = t * ax * ay + s * az; r[5] = t * ay * ay + c;      r[6] = t * ay * az - s * ax; r[7] = 0f;
        r[8] = t * ax * az - s * ay; r[9] = t * ay * az + s * ax; r[10] = t * az * az + c;     r[11] = 0f;
        r[12] = 0f; r[13] = 0f; r[14] = 0f; r[15] = 1f;

        postMultiply(r);
    }

    /** this = this * other (other given as a flat row-major 16-float matrix). */
    private void postMultiply(float[] o) {
        float[] result = new float[16];
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                float sum = 0f;
                for (int k = 0; k < 4; k++) {
                    sum += m[row * 4 + k] * o[k * 4 + col];
                }
                result[row * 4 + col] = sum;
            }
        }
        System.arraycopy(result, 0, m, 0, 16);
    }

    public void postMultiply(Transform t) {
        postMultiply(t.m);
    }

    /** Inverts this matrix in place (general 4x4 Gauss-Jordan with partial pivoting). */
    public void invert() {
        float[][] a = new float[4][8];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                a[r][c] = m[r * 4 + c];
            }
            a[r][4 + r] = 1f;
        }
        for (int col = 0; col < 4; col++) {
            int pivot = col;
            float best = Math.abs(a[col][col]);
            for (int r = col + 1; r < 4; r++) {
                float v = Math.abs(a[r][col]);
                if (v > best) { best = v; pivot = r; }
            }
            if (pivot != col) {
                float[] tmp = a[col]; a[col] = a[pivot]; a[pivot] = tmp;
            }
            float d = a[col][col];
            if (Math.abs(d) < 1e-12f) {
                setIdentity();
                return;
            }
            for (int c = 0; c < 8; c++) {
                a[col][c] /= d;
            }
            for (int r = 0; r < 4; r++) {
                if (r == col) continue;
                float f = a[r][col];
                if (f == 0f) continue;
                for (int c = 0; c < 8; c++) {
                    a[r][c] -= f * a[col][c];
                }
            }
        }
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                m[r * 4 + c] = a[r][4 + c];
            }
        }
    }

    /** Transforms one or more 4-float vectors in place: v' = M * v. */
    public void transform(float[] vectors) {
        float[] v = new float[4];
        for (int base = 0; base + 3 < vectors.length; base += 4) {
            v[0] = vectors[base]; v[1] = vectors[base + 1];
            v[2] = vectors[base + 2]; v[3] = vectors[base + 3];
            for (int row = 0; row < 4; row++) {
                int r = row * 4;
                vectors[base + row] = m[r] * v[0] + m[r + 1] * v[1] + m[r + 2] * v[2] + m[r + 3] * v[3];
            }
        }
    }
}
