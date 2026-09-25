/*
 * Pixel-art upscaling for the PS4 display (built with -O2 by
 * ps4/build_cldc.sh; MIDP itself is a -O0 debug build).
 */
#include <stdint.h>

/*
 * Scale2x (AdvMAME2x): doubles a w x h image of 32-bit pixels, rounding
 * the corners of diagonal edges instead of repeating each pixel. Pitches
 * are in pixels; dst must hold 2w x 2h.
 */
void psn_scale2x(const uint32_t *src, int w, int h, int spitch,
                 uint32_t *dst, int dpitch)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const uint32_t *row = src + y * spitch;
        const uint32_t *up = y > 0 ? row - spitch : row;
        const uint32_t *down = y < h - 1 ? row + spitch : row;
        uint32_t *out0 = dst + 2 * y * dpitch;
        uint32_t *out1 = out0 + dpitch;
        for (x = 0; x < w; x++) {
            int l = x > 0 ? x - 1 : x, r = x < w - 1 ? x + 1 : x;
            uint32_t p = row[x];
            uint32_t a = up[x], b = row[r], c = row[l], d = down[x];
            if (a != d && c != b) {
                out0[2 * x]     = c == a ? c : p;
                out0[2 * x + 1] = a == b ? b : p;
                out1[2 * x]     = c == d ? c : p;
                out1[2 * x + 1] = d == b ? b : p;
            } else {
                out0[2 * x] = out0[2 * x + 1] = p;
                out1[2 * x] = out1[2 * x + 1] = p;
            }
        }
    }
}
