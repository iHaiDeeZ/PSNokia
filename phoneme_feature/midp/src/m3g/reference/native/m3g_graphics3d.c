/*
 * Minimal software 3D rasterizer backing javax.microedition.m3g.Graphics3D.
 * Renders directly into the same shared native framebuffer this port's
 * 2D graphics already writes into (gxj_system_screen_buffer, RGB565) -
 * this is what makes Background.setColorClearEnable(false) (draw 3D on
 * top of already-painted 2D content, the pattern real bundled MIDlets
 * use) work for free, with no separate offscreen-buffer compositing
 * step needed. Scoped to what those MIDlets actually exercise: no
 * lighting, no blending beyond texture-replace, no near-plane clipping
 * (triangles fully behind the camera are dropped instead) - see M3G
 * build notes for the full list of simplifications.
 */

#include <kni.h>
#include <commonKNIMacros.h>
#include <gxj_putpixel.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <psp2/io/fcntl.h>
#include <stdio.h>

/*
 * Diagnostic-only: this native rasterizer has never actually been exercised
 * with real triangle data before this session (every earlier bug kept the
 * pipeline from ever reaching it with valid input) - counting what happens
 * to triangles/pixels here directly, rather than assuming this code is
 * correct just because it looks structurally reasonable. Throttled to the
 * first handful of nDrawTriangles calls, safe to leave.
 */
static int m3g_diag_totalCalls = 0;
static int m3g_diag_totalTris = 0;
static int m3g_diag_totalBehindCam = 0;
static int m3g_diag_totalCulled = 0;
static int m3g_diag_totalOffscreenBox = 0;
static int m3g_diag_totalPixelsWritten = 0;
static int m3g_diag_everWrotePixel = 0;
static void m3g_diag_log(const char *msg) {
    char buf[160];
    int len = sprintf(buf, "%s\n", msg);
    int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, buf, len);
        sceIoClose(fd);
    }
}

#define JavaFloatArray(__handle) \
    ( (jfloat*) &(unhand(jfloat_array, (__handle))->elements[0]) )

static float *m3g_depth = NULL;
static int m3g_depth_w = 0, m3g_depth_h = 0;

static void ensure_depth_buffer(int w, int h) {
    if (m3g_depth == NULL || m3g_depth_w != w || m3g_depth_h != h) {
        if (m3g_depth != NULL) {
            free(m3g_depth);
        }
        m3g_depth = (float *) malloc((size_t) w * h * sizeof(float));
        m3g_depth_w = w;
        m3g_depth_h = h;
    }
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Graphics3D_nBindTarget) {
    /* Only the primary display surface is supported as a render target -
     * real bundled MIDlets tested so far only bind the Canvas's own
     * paint() Graphics, never an offscreen Image. */
    ensure_depth_buffer(gxj_system_screen_buffer.width, gxj_system_screen_buffer.height);
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Graphics3D_nReleaseTarget) {
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Graphics3D_nClear) {
    jboolean depthClear = KNI_GetParameterAsBoolean(3);
    jint colorRGB = KNI_GetParameterAsInt(2);
    jboolean colorClear = KNI_GetParameterAsBoolean(1);
    int w = gxj_system_screen_buffer.width;
    int h = gxj_system_screen_buffer.height;

    if (colorClear && gxj_system_screen_buffer.pixelData != NULL) {
        gxj_pixel_type px = (gxj_pixel_type) GXJ_RGB24TORGB16(colorRGB);
        int i, n = w * h;
        gxj_pixel_type *p = gxj_system_screen_buffer.pixelData;
        for (i = 0; i < n; i++) {
            p[i] = px;
        }
    }
    if (depthClear && m3g_depth != NULL) {
        int i, n = m3g_depth_w * m3g_depth_h;
        for (i = 0; i < n; i++) {
            m3g_depth[i] = 1e9f;
        }
    }
    KNI_ReturnVoid();
}

/* Row-major 4x4 * 4-vector: out = M * v */
static void mat4_transform(const float *m, const float *v, float *out) {
    int r;
    for (r = 0; r < 4; r++) {
        const float *row = m + r * 4;
        out[r] = row[0] * v[0] + row[1] * v[1] + row[2] * v[2] + row[3] * v[3];
    }
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Graphics3D_nDrawTriangles) {
    jint defaultColor = KNI_GetParameterAsInt(10);
    jint wrapMode = KNI_GetParameterAsInt(7);
    jint texHeight = KNI_GetParameterAsInt(6);
    jint texWidth = KNI_GetParameterAsInt(5);

    KNI_StartHandles(6);
    KNI_DeclareHandle(positionsH);
    KNI_DeclareHandle(texCoordsH);
    KNI_DeclareHandle(trisH);
    KNI_DeclareHandle(texPixelsH);
    KNI_DeclareHandle(modelViewH);
    KNI_DeclareHandle(projH);

    KNI_GetParameterAsObject(1, positionsH);
    KNI_GetParameterAsObject(2, texCoordsH);
    KNI_GetParameterAsObject(3, trisH);
    KNI_GetParameterAsObject(4, texPixelsH);
    KNI_GetParameterAsObject(8, modelViewH);
    KNI_GetParameterAsObject(9, projH);

    if (!KNI_IsNullHandle(positionsH) && !KNI_IsNullHandle(trisH)
            && !KNI_IsNullHandle(modelViewH) && !KNI_IsNullHandle(projH)
            && gxj_system_screen_buffer.pixelData != NULL && m3g_depth != NULL) {

        jfloat *positions = JavaFloatArray(positionsH);
        jfloat *texCoords = KNI_IsNullHandle(texCoordsH) ? NULL : JavaFloatArray(texCoordsH);
        jint *tris = JavaIntArray(trisH);
        jint triCount = KNI_GetArrayLength(trisH) / 3;
        jint *texPixels = KNI_IsNullHandle(texPixelsH) ? NULL : JavaIntArray(texPixelsH);
        jfloat *mv = JavaFloatArray(modelViewH);
        jfloat *proj = JavaFloatArray(projH);

        int screenW = gxj_system_screen_buffer.width;
        int screenH = gxj_system_screen_buffer.height;
        gxj_pixel_type *fb = gxj_system_screen_buffer.pixelData;
        int t;
        int diagBehindCam = 0, diagCulled = 0, diagOffscreenBox = 0, diagPixelsWritten = 0;

        for (t = 0; t < triCount; t++) {
            int i0 = tris[t * 3], i1 = tris[t * 3 + 1], i2 = tris[t * 3 + 2];
            float obj[3][4], eye[3][4], clip[3][4];
            float sx[3], sy[3], sz[3], sw[3];
            int k, vi[3];
            vi[0] = i0; vi[1] = i1; vi[2] = i2;

            for (k = 0; k < 3; k++) {
                obj[k][0] = positions[vi[k] * 3];
                obj[k][1] = positions[vi[k] * 3 + 1];
                obj[k][2] = positions[vi[k] * 3 + 2];
                obj[k][3] = 1.0f;
                mat4_transform(mv, obj[k], eye[k]);
                mat4_transform(proj, eye[k], clip[k]);
            }

            if (clip[0][3] <= 0.01f || clip[1][3] <= 0.01f || clip[2][3] <= 0.01f) {
                diagBehindCam++;
                continue; /* behind or too close to the camera - no near-plane clip, just drop */
            }

            for (k = 0; k < 3; k++) {
                float invW = 1.0f / clip[k][3];
                sx[k] = screenW * 0.5f * (1.0f + clip[k][0] * invW);
                sy[k] = screenH * 0.5f * (1.0f - clip[k][1] * invW);
                sz[k] = clip[k][2] * invW;
                sw[k] = invW; /* 1/w, for perspective-correct UV interpolation */
            }

            /* Backface cull (screen space is Y-down, so front faces wind clockwise here). */
            float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (area >= 0.0f) {
                diagCulled++;
                continue;
            }

            int rawMinX = (int) floorf(sx[0] < sx[1] ? (sx[0] < sx[2] ? sx[0] : sx[2]) : (sx[1] < sx[2] ? sx[1] : sx[2]));
            int rawMaxX = (int) ceilf(sx[0] > sx[1] ? (sx[0] > sx[2] ? sx[0] : sx[2]) : (sx[1] > sx[2] ? sx[1] : sx[2]));
            int rawMinY = (int) floorf(sy[0] < sy[1] ? (sy[0] < sy[2] ? sy[0] : sy[2]) : (sy[1] < sy[2] ? sy[1] : sy[2]));
            int rawMaxY = (int) ceilf(sy[0] > sy[1] ? (sy[0] > sy[2] ? sy[0] : sy[2]) : (sy[1] > sy[2] ? sy[1] : sy[2]));
            if (rawMaxX < 0 || rawMinX >= screenW || rawMaxY < 0 || rawMinY >= screenH) {
                diagOffscreenBox++;
            }
            int minX = (int) clampf((float) rawMinX, 0, screenW - 1);
            int maxX = (int) clampf((float) rawMaxX, 0, screenW - 1);
            int minY = (int) clampf((float) rawMinY, 0, screenH - 1);
            int maxY = (int) clampf((float) rawMaxY, 0, screenH - 1);

            float invArea = 1.0f / area;
            int px, py;
            for (py = minY; py <= maxY; py++) {
                for (px = minX; px <= maxX; px++) {
                    float cx = px + 0.5f, cy = py + 0.5f;
                    float w0 = ((sx[1] - cx) * (sy[2] - cy) - (sx[2] - cx) * (sy[1] - cy)) * invArea;
                    float w1 = ((sx[2] - cx) * (sy[0] - cy) - (sx[0] - cx) * (sy[2] - cy)) * invArea;
                    float w2 = 1.0f - w0 - w1;
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                        continue;
                    }
                    float depth = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                    int didx = py * screenW + px;
                    if (depth >= m3g_depth[didx]) {
                        continue;
                    }

                    int argb;
                    if (texPixels != NULL && texCoords != NULL) {
                        float pw = w0 * sw[0] + w1 * sw[1] + w2 * sw[2];
                        float u = (w0 * texCoords[vi[0] * 2] * sw[0]
                                 + w1 * texCoords[vi[1] * 2] * sw[1]
                                 + w2 * texCoords[vi[2] * 2] * sw[2]) / pw;
                        float v = (w0 * texCoords[vi[0] * 2 + 1] * sw[0]
                                 + w1 * texCoords[vi[1] * 2 + 1] * sw[1]
                                 + w2 * texCoords[vi[2] * 2 + 1] * sw[2]) / pw;
                        if (wrapMode == 240 /* WRAP_CLAMP */) {
                            u = clampf(u, 0.0f, 1.0f);
                            v = clampf(v, 0.0f, 1.0f);
                        } else {
                            u = u - floorf(u);
                            v = v - floorf(v);
                        }
                        int tx = (int) (u * texWidth);
                        int ty = (int) (v * texHeight);
                        if (tx >= texWidth) tx = texWidth - 1;
                        if (ty >= texHeight) ty = texHeight - 1;
                        if (tx < 0) tx = 0;
                        if (ty < 0) ty = 0;
                        argb = texPixels[ty * texWidth + tx];
                        if (((argb >> 24) & 0xFF) < 0x40) {
                            continue; /* mostly-transparent texel: skip (no real alpha blending) */
                        }
                    } else {
                        argb = defaultColor;
                    }

                    m3g_depth[didx] = depth;
                    fb[didx] = (gxj_pixel_type) GXJ_RGB24TORGB16(argb & 0x00FFFFFF);
                    diagPixelsWritten++;
                }
            }
        }

        /*
         * Accumulate into GLOBAL totals across every call (not per-call
         * logging, which exhausted its old low cap during the menu screens
         * alone, before any real gameplay data could ever be captured) and
         * print one aggregate summary every 500 calls - bounds file I/O to
         * a level already proven safe elsewhere in this project, while
         * covering a much longer real-world window (through menus AND into
         * actual gameplay) than a low per-call cap ever could.
         */
        m3g_diag_totalCalls++;
        m3g_diag_totalTris += (int) triCount;
        m3g_diag_totalBehindCam += diagBehindCam;
        m3g_diag_totalCulled += diagCulled;
        m3g_diag_totalOffscreenBox += diagOffscreenBox;
        m3g_diag_totalPixelsWritten += diagPixelsWritten;
        if (m3g_diag_totalPixelsWritten > 0 && !m3g_diag_everWrotePixel) {
            m3g_diag_everWrotePixel = 1;
            m3g_diag_log("M3G_RASTER_FIRST_PIXEL_WRITTEN");
        }
        if (m3g_diag_totalCalls % 500 == 0) {
            char line[200];
            sprintf(line, "M3G_RASTER_SUMMARY calls=%d tris=%d behindCam=%d culled=%d offscreenBox=%d pixelsWritten=%d screen=%dx%d",
                    m3g_diag_totalCalls, m3g_diag_totalTris, m3g_diag_totalBehindCam, m3g_diag_totalCulled,
                    m3g_diag_totalOffscreenBox, m3g_diag_totalPixelsWritten, screenW, screenH);
            m3g_diag_log(line);
        }
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
}
