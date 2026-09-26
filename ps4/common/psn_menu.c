// The PSNokia menu (see psn_menu.h), in the spirit of a console's home
// menu: a deep blue background with slowly moving waves, the game on the
// left and the options on the right. Everything is drawn in software into
// a 32-bit buffer, which is then copied to the window.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// libSceFreeType exports these, but the toolchain's freetype.h lacks them
FT_Error FT_Init_FreeType(FT_Library* alibrary);
FT_Error FT_New_Face(FT_Library library, const char* filepathname, FT_Long face_index,
                     FT_Face* aface);
FT_Error FT_Load_Char(FT_Face face, FT_ULong char_code, FT_Int32 load_flags);

#include "psn_menu.h"

int32_t sceSysmoduleLoadModule(uint16_t id);
#define SYSMODULE_FREETYPE_OL 0x009A

// The package's font and game icon (make_game.sh puts them there)
#ifndef PSN_MENU_ASSETS
#define PSN_MENU_ASSETS "/app0/assets"
#endif

void write_marker(const char* text, int len);

static void menu_log(const char* text) {
    write_marker(text, (int)strlen(text));
}

// --- Frame buffer ------------------------------------------------------------

static uint32_t* fb;            // 0x00RRGGBB
static int fbW, fbH;

static void blend(int x, int y, uint32_t rgb, int alpha) {
    if (x < 0 || y < 0 || x >= fbW || y >= fbH || alpha <= 0) {
        return;
    }
    uint32_t* p = &fb[y * fbW + x];
    if (alpha >= 255) {
        *p = rgb;
        return;
    }
    uint32_t d = *p;
    int ia = 255 - alpha;
    uint32_t r = (((rgb >> 16) & 0xff) * alpha + ((d >> 16) & 0xff) * ia) / 255;
    uint32_t g = (((rgb >> 8) & 0xff) * alpha + ((d >> 8) & 0xff) * ia) / 255;
    uint32_t b = ((rgb & 0xff) * alpha + (d & 0xff) * ia) / 255;
    *p = (r << 16) | (g << 8) | b;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

// Rounded rectangle, filled, with anti-aliased corners
static void fill_round_rect(int x, int y, int w, int h, int radius, uint32_t rgb, int alpha) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            float cx = clampf((float)xx + 0.5f, (float)(x + radius), (float)(x + w - radius));
            float cy = clampf((float)yy + 0.5f, (float)(y + radius), (float)(y + h - radius));
            float dx = xx + 0.5f - cx, dy = yy + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy) - radius;
            float cover = clampf(0.5f - d, 0.0f, 1.0f);
            blend(xx, yy, rgb, (int)(alpha * cover));
        }
    }
}

// --- Background -------------------------------------------------------------

static uint32_t* bgRows;        // the gradient, one colour per row

static void make_background(void) {
    bgRows = (uint32_t*)malloc(sizeof(uint32_t) * fbH);
    for (int y = 0; y < fbH; y++) {
        float t = (float)y / (fbH - 1);
        // #0A1A3A at the top to #13407A at the bottom
        int r = (int)(0x0a + (0x13 - 0x0a) * t);
        int g = (int)(0x1a + (0x40 - 0x1a) * t);
        int b = (int)(0x3a + (0x7a - 0x3a) * t);
        bgRows[y] = (r << 16) | (g << 8) | b;
    }
}

static float wave_y(int wave, float x, float t) {
    float base = fbH * (0.81f + 0.035f * wave);
    return base + 36.0f * sinf(x * 0.0021f + t * (0.35f + 0.08f * wave) + wave * 1.7f)
                + 16.0f * sinf(x * 0.0047f - t * (0.22f + 0.05f * wave) + wave * 0.6f);
}

static void draw_background(float t) {
    for (int y = 0; y < fbH; y++) {
        uint32_t c = bgRows[y];
        uint32_t* row = &fb[y * fbW];
        for (int x = 0; x < fbW; x++) {
            row[x] = c;
        }
    }
    // A faint band between the first and last wave, then the waves
    for (int x = 0; x < fbW; x++) {
        int top = (int)wave_y(0, (float)x, t), bottom = (int)wave_y(2, (float)x, t);
        if (top > bottom) {
            int s = top; top = bottom; bottom = s;
        }
        for (int y = top; y < bottom; y++) {
            blend(x, y, 0x5fa8ff, 14);
        }
        static const uint32_t colours[3] = { 0x6fb3ff, 0x9fcbff, 0xc8e2ff };
        static const int strength[3] = { 150, 110, 80 };
        for (int w = 0; w < 3; w++) {
            float wy = wave_y(w, (float)x, t);
            for (int y = (int)wy - 3; y <= (int)wy + 3; y++) {
                float cover = 1.0f - fabsf(y + 0.5f - wy) / 2.2f;
                if (cover > 0) {
                    blend(x, y, colours[w], (int)(strength[w] * cover));
                }
            }
        }
    }
}

// --- Text ---------------------------------------------------------------------

enum { FONT_SMALL, FONT_MEDIUM, FONT_LARGE, FONT_COUNT };
static const int fontPixels[FONT_COUNT] = { 30, 40, 50 };

typedef struct {
    int w, h, left, top, advance;
    unsigned char* bitmap;
} glyph;

// Latin-1, plus two slots for the single angle quotes
#define GLYPH_LSAQUO 1
#define GLYPH_RSAQUO 2
static glyph glyphs[FONT_COUNT][256];
static int fontAscent[FONT_COUNT];
static int haveFont;

static int glyph_slot(unsigned cp) {
    if (cp == 0x2039) return GLYPH_LSAQUO;
    if (cp == 0x203a) return GLYPH_RSAQUO;
    return cp < 256 && cp >= 32 ? (int)cp : '?';
}

static void load_fonts(void) {
    FT_Library lib;
    FT_Face face;
    char line[160];
    int rc = sceSysmoduleLoadModule(SYSMODULE_FREETYPE_OL);
    if (FT_Init_FreeType(&lib) != 0) {
        snprintf(line, sizeof(line), "MENU: FreeType init failed (sysmodule %x)\n", rc);
        menu_log(line);
        return;
    }
    if (FT_New_Face(lib, PSN_MENU_ASSETS "/fonts/Gontserrat-Regular.ttf", 0, &face) != 0) {
        menu_log("MENU: font not found\n");
        return;
    }
    for (int f = 0; f < FONT_COUNT; f++) {
        FT_Set_Pixel_Sizes(face, 0, fontPixels[f]);
        fontAscent[f] = (int)(face->size->metrics.ascender >> 6);
        for (int slot = 1; slot < 256; slot++) {
            unsigned cp = slot == GLYPH_LSAQUO ? 0x2039 : slot == GLYPH_RSAQUO ? 0x203a
                          : (unsigned)slot;
            if (slot >= 3 && slot < 32) {
                continue;
            }
            if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) {
                continue;
            }
            FT_GlyphSlot g = face->glyph;
            glyph* out = &glyphs[f][slot];
            out->w = g->bitmap.width;
            out->h = g->bitmap.rows;
            out->left = g->bitmap_left;
            out->top = g->bitmap_top;
            out->advance = (int)(g->advance.x >> 6);
            if (out->w > 0 && out->h > 0) {
                out->bitmap = (unsigned char*)malloc(out->w * out->h);
                for (int r = 0; r < out->h; r++) {
                    memcpy(out->bitmap + r * out->w, g->bitmap.buffer + r * g->bitmap.pitch, out->w);
                }
            }
        }
    }
    haveFont = 1;
}

// Next code point of a UTF-8 string
static unsigned next_char(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    unsigned c = *p++;
    if (c >= 0xe0 && p[0] && p[1]) {
        c = ((c & 0x0f) << 12) | ((p[0] & 0x3f) << 6) | (p[1] & 0x3f);
        p += 2;
    } else if (c >= 0xc0 && p[0]) {
        c = ((c & 0x1f) << 6) | (p[0] & 0x3f);
        p += 1;
    }
    *s = (const char*)p;
    return c;
}

static int text_width(int font, const char* s) {
    int w = 0;
    while (*s) {
        w += glyphs[font][glyph_slot(next_char(&s))].advance;
    }
    return w;
}

// Draws s with its top at y (the font's ascent below y is the baseline)
static void draw_text(int font, int x, int y, const char* s, uint32_t rgb, int alpha) {
    int baseline = y + fontAscent[font];
    while (*s) {
        glyph* g = &glyphs[font][glyph_slot(next_char(&s))];
        for (int r = 0; r < g->h; r++) {
            for (int c = 0; c < g->w; c++) {
                int a = g->bitmap[r * g->w + c];
                if (a) {
                    blend(x + g->left + c, baseline - g->top + r, rgb, a * alpha / 255);
                }
            }
        }
        x += g->advance;
    }
}

static void draw_text_centered(int font, int cx, int y, const char* s, uint32_t rgb, int alpha) {
    draw_text(font, cx - text_width(font, s) / 2, y, s, rgb, alpha);
}

static void draw_text_right(int font, int right, int y, const char* s, uint32_t rgb, int alpha) {
    draw_text(font, right - text_width(font, s), y, s, rgb, alpha);
}

// --- Icons ------------------------------------------------------------------------
// Drawn from distance functions in a unit square (-1..1), anti-aliased.

enum { ICON_PLAY, ICON_RESTART, ICON_GAMEPAD, ICON_ASPECT, ICON_PICTURE, ICON_POWER,
       ICON_BACK, ICON_CROSS, ICON_CIRCLE };

static float seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float t = clampf((wx * vx + wy * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
    float dx = wx - vx * t, dy = wy - vy * t;
    return sqrtf(dx * dx + dy * dy);
}

// Distance to a circular arc of radius r, leaving out a gap of gapHalf
// radians around the direction gapAngle
static float arc_dist(float px, float py, float r, float gapAngle, float gapHalf) {
    float a = atan2f(py, px) - gapAngle;
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    if (fabsf(a) >= gapHalf) {
        return fabsf(sqrtf(px * px + py * py) - r);
    }
    float e1x = r * cosf(gapAngle + gapHalf), e1y = r * sinf(gapAngle + gapHalf);
    float e2x = r * cosf(gapAngle - gapHalf), e2y = r * sinf(gapAngle - gapHalf);
    float d1 = sqrtf((px - e1x) * (px - e1x) + (py - e1y) * (py - e1y));
    float d2 = sqrtf((px - e2x) * (px - e2x) + (py - e2y) * (py - e2y));
    return d1 < d2 ? d1 : d2;
}

static float box_dist(float px, float py, float hw, float hh, float r) {
    float qx = fabsf(px) - hw + r, qy = fabsf(py) - hh + r;
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float inside = qx > qy ? qx : qy;
    return sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
}

// Coverage of an icon at unit-square point (px, py); stroke is half the
// line width in the same units
static float icon_cover(int icon, float px, float py, float stroke, float aa) {
    float d = 1e9f;
    switch (icon) {
    case ICON_PLAY: {       // filled triangle pointing right
        float e1 = seg_dist(px, py, -0.45f, -0.6f, -0.45f, 0.6f);
        float e2 = seg_dist(px, py, -0.45f, -0.6f, 0.65f, 0.0f);
        float e3 = seg_dist(px, py, -0.45f, 0.6f, 0.65f, 0.0f);
        int inside = px > -0.45f && py > -0.6f + (px + 0.45f) * (0.6f / 1.1f)
                     && py < 0.6f - (px + 0.45f) * (0.6f / 1.1f);
        float e = fminf(e1, fminf(e2, e3));
        return inside ? 1.0f : clampf(0.5f - e / aa, 0.0f, 1.0f);
    }
    case ICON_RESTART: {    // open circle with an arrow head
        d = arc_dist(px, py, 0.62f, -1.2f, 0.45f);
        float hx = 0.62f * cosf(-0.75f), hy = 0.62f * sinf(-0.75f);
        d = fminf(d, seg_dist(px, py, hx, hy, hx - 0.05f, hy - 0.42f));
        d = fminf(d, seg_dist(px, py, hx, hy, hx - 0.42f, hy + 0.02f));
        break;
    }
    case ICON_GAMEPAD:      // controller body, d-pad and two buttons
        d = fabsf(box_dist(px, py + 0.05f, 0.85f, 0.45f, 0.4f));
        d = fminf(d, seg_dist(px, py, -0.62f, -0.05f, -0.22f, -0.05f));
        d = fminf(d, seg_dist(px, py, -0.42f, -0.25f, -0.42f, 0.15f));
        d = fminf(d, sqrtf((px - 0.3f) * (px - 0.3f) + (py - 0.05f) * (py - 0.05f)) - 0.06f);
        d = fminf(d, sqrtf((px - 0.52f) * (px - 0.52f) + (py + 0.17f) * (py + 0.17f)) - 0.06f);
        break;
    case ICON_ASPECT:       // screen with corner marks
        d = fabsf(box_dist(px, py, 0.85f, 0.55f, 0.12f));
        d = fminf(d, seg_dist(px, py, -0.55f, -0.25f, -0.55f, -0.05f));
        d = fminf(d, seg_dist(px, py, -0.55f, -0.25f, -0.35f, -0.25f));
        d = fminf(d, seg_dist(px, py, 0.55f, 0.25f, 0.55f, 0.05f));
        d = fminf(d, seg_dist(px, py, 0.55f, 0.25f, 0.35f, 0.25f));
        break;
    case ICON_PICTURE:      // frame with a mountain and a sun
        d = fabsf(box_dist(px, py, 0.85f, 0.6f, 0.12f));
        d = fminf(d, seg_dist(px, py, -0.6f, 0.4f, -0.15f, -0.1f));
        d = fminf(d, seg_dist(px, py, -0.15f, -0.1f, 0.2f, 0.25f));
        d = fminf(d, seg_dist(px, py, 0.2f, 0.25f, 0.6f, 0.4f));
        d = fminf(d, fabsf(sqrtf((px - 0.4f) * (px - 0.4f) + (py + 0.25f) * (py + 0.25f)) - 0.12f));
        break;
    case ICON_POWER:        // open circle with a line through the gap
        d = arc_dist(px, py, 0.6f, -1.5708f, 0.55f);
        d = fminf(d, seg_dist(px, py, 0.0f, -0.8f, 0.0f, -0.15f));
        break;
    case ICON_BACK:         // arrow pointing left
        d = seg_dist(px, py, -0.6f, 0.0f, 0.6f, 0.0f);
        d = fminf(d, seg_dist(px, py, -0.6f, 0.0f, -0.2f, -0.4f));
        d = fminf(d, seg_dist(px, py, -0.6f, 0.0f, -0.2f, 0.4f));
        break;
    case ICON_CROSS:        // the DS4 cross button: ring with an X
        d = fabsf(sqrtf(px * px + py * py) - 0.85f);
        d = fminf(d, seg_dist(px, py, -0.35f, -0.35f, 0.35f, 0.35f));
        d = fminf(d, seg_dist(px, py, -0.35f, 0.35f, 0.35f, -0.35f));
        break;
    case ICON_CIRCLE:       // the DS4 circle button: ring with a circle
        d = fabsf(sqrtf(px * px + py * py) - 0.85f);
        d = fminf(d, fabsf(sqrtf(px * px + py * py) - 0.38f));
        break;
    }
    return clampf(0.5f - (d - stroke) / aa, 0.0f, 1.0f);
}

static void draw_icon(int icon, int cx, int cy, int size, uint32_t rgb, int alpha) {
    float half = size / 2.0f;
    float unit = 1.0f / half;           // one pixel in unit-square units
    for (int y = cy - size / 2 - 2; y <= cy + size / 2 + 2; y++) {
        for (int x = cx - size / 2 - 2; x <= cx + size / 2 + 2; x++) {
            float px = (x + 0.5f - cx) / half, py = (y + 0.5f - cy) / half;
            float c = icon_cover(icon, px, py, 0.09f, unit);
            if (c > 0) {
                blend(x, y, rgb, (int)(alpha * c));
            }
        }
    }
}

// --- The game on the left ------------------------------------------------------

static uint32_t* iconPixels;    // 0xAARRGGBB
static int iconW, iconH;

static void load_game_icon(void) {
    FILE* f = fopen(PSN_MENU_ASSETS "/icon.raw", "rb");
    uint32_t size[2];
    if (f == NULL) {
        return;
    }
    if (fread(size, 4, 2, f) == 2 && size[0] > 0 && size[0] <= 512 && size[1] > 0
            && size[1] <= 512) {
        iconPixels = (uint32_t*)malloc(size[0] * size[1] * 4);
        if (iconPixels && fread(iconPixels, 4, size[0] * size[1], f) == size[0] * size[1]) {
            iconW = (int)size[0];
            iconH = (int)size[1];
        }
    }
    fclose(f);
}

// Copies the icon into a size x size rounded square centred at cx, cy
static void draw_game_icon(int cx, int cy, int size) {
    int x0 = cx - size / 2, y0 = cy - size / 2;
    int radius = size / 7;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float qx = clampf(x + 0.5f, (float)radius, (float)(size - radius));
            float qy = clampf(y + 0.5f, (float)radius, (float)(size - radius));
            float dx = x + 0.5f - qx, dy = y + 0.5f - qy;
            float cover = clampf(radius + 0.5f - sqrtf(dx * dx + dy * dy), 0.0f, 1.0f);
            if (cover <= 0) {
                continue;
            }
            if (iconPixels == NULL) {
                blend(x0 + x, y0 + y, 0x2a5c9a, (int)(255 * cover));
                continue;
            }
            uint32_t p = iconPixels[(y * iconH / size) * iconW + (x * iconW / size)];
            blend(x0 + x, y0 + y, p & 0xffffff, (int)(((p >> 24) & 0xff) * cover));
        }
    }
    if (iconPixels == NULL) {
        draw_icon(ICON_GAMEPAD, cx, cy, size / 2, 0xe6f1fb, 230);
    }
}

// The paused game's frame, scaled into a box of the shape the display uses
static SDL_Surface* frameCopy;

static void draw_game_frame(const psn_menu_host* host, int cx, int cy, int maxW, int maxH) {
    int w, h;
    const char* view = host->view_name(host->get_view());
    if (frameCopy == NULL) {
        return;
    }
    if (strcmp(view, "Full") == 0) {
        w = maxW; h = maxW * 9 / 16;
    } else if (strcmp(view, "4:3") == 0) {
        h = maxH; w = maxH * 4 / 3;
    } else {
        w = maxW; h = frameCopy->h * maxW / frameCopy->w;
    }
    if (h > maxH) { w = w * maxH / h; h = maxH; }
    if (w > maxW) { h = h * maxW / w; w = maxW; }
    int x0 = cx - w / 2, y0 = cy - h / 2;
    fill_round_rect(x0 - 6, y0 - 6, w + 12, h + 12, 10, 0x000000, 90);
    const uint32_t* src = (const uint32_t*)frameCopy->pixels;
    int pitch = frameCopy->pitch / 4;
    for (int y = 0; y < h; y++) {
        const uint32_t* row = src + (y * frameCopy->h / h) * pitch;
        for (int x = 0; x < w; x++) {
            blend(x0 + x, y0 + y, row[x * frameCopy->w / w] & 0xffffff, 255);
        }
    }
}

// --- Menu state ------------------------------------------------------------------

enum { ITEM_START, ITEM_RESTART, ITEM_BUTTONS, ITEM_VIEW, ITEM_PICTURE, ITEM_CLOSE, ITEM_COUNT };

// The phone keys a button can send, and their names
static const int keyCodes[] = { -1, -2, -3, -4, -5, -6, -7, '0', '1', '2', '3', '4', '5',
                                '6', '7', '8', '9', '*', '#', -8, 0 };
static const char* const keyNames[] = { "Up", "Down", "Left", "Right", "Fire / select",
    "Left soft key", "Right soft key", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "*", "#", "Clear", "Nothing" };
#define KEY_CHOICES ((int)(sizeof(keyCodes) / sizeof(keyCodes[0])))

static int key_index(int code) {
    for (int i = 0; i < KEY_CHOICES; i++) {
        if (keyCodes[i] == code) {
            return i;
        }
    }
    return KEY_CHOICES - 1;
}

typedef struct {
    int inGame;
    int screen;             // 0 main, 1 buttons
    int selected;           // main list
    int buttonSel;          // buttons list (last row: reset)
    int buttonTop;          // first visible button row
    int confirmClose;
    float highlightY;       // animated highlight position
} menu_state;

#define LIST_X 860
#define LIST_W 900
#define MAIN_TOP 250
#define MAIN_STEP 100
#define BTN_TOP 300
#define BTN_STEP 76
#define BTN_ROWS 8

static const uint32_t TEXT_BRIGHT = 0xffffff, TEXT_NORMAL = 0xc9dcf5, TEXT_DIM = 0x7f9cc4;

static int item_enabled(const menu_state* m, int item) {
    return item != ITEM_RESTART || m->inGame;
}

static void draw_value(int y, const char* value, int selected) {
    char text[64];
    snprintf(text, sizeof(text), "\xe2\x80\xb9  %s  \xe2\x80\xba", value);
    draw_text_right(FONT_SMALL, LIST_X + LIST_W - 40, y, text,
                    selected ? TEXT_BRIGHT : TEXT_DIM, 255);
}

static void draw_main(const menu_state* m, const psn_menu_host* host) {
    static const int icons[ITEM_COUNT] = { ICON_PLAY, ICON_RESTART, ICON_GAMEPAD, ICON_ASPECT,
                                           ICON_PICTURE, ICON_POWER };
    const char* labels[ITEM_COUNT] = {
        m->inGame ? "Return to the game" : "Start the game", "Restart the game",
        "Assign buttons", "Aspect ratio", "Picture",
        m->confirmClose ? "Close game?  Press again" : "Close game" };
    fill_round_rect(LIST_X - 20, (int)m->highlightY - 14, LIST_W, 88, 18, 0xe6f1ff, 34);
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = MAIN_TOP + i * MAIN_STEP;
        int sel = i == m->selected;
        int enabled = item_enabled(m, i);
        uint32_t colour = !enabled ? TEXT_DIM : sel ? TEXT_BRIGHT : TEXT_NORMAL;
        int alpha = enabled ? 255 : 110;
        draw_icon(icons[i], LIST_X + 40, y + 30, sel ? 46 : 40, colour, alpha);
        draw_text(sel ? FONT_LARGE : FONT_MEDIUM, LIST_X + 100, sel ? y - 2 : y + 4,
                  labels[i], colour, alpha);
        if (i == ITEM_VIEW) {
            draw_value(y + 12, host->view_name(host->get_view()), sel);
        } else if (i == ITEM_PICTURE) {
            draw_value(y + 12, host->get_smooth() ? "Smooth" : "Sharp", sel);
        }
    }
}

static void draw_buttons(const menu_state* m, const psn_menu_host* host) {
    int count = host->button_count();
    draw_icon(ICON_GAMEPAD, LIST_X + 40, 200, 44, TEXT_BRIGHT, 255);
    draw_text(FONT_LARGE, LIST_X + 100, 170, "Assign buttons", TEXT_BRIGHT, 255);
    fill_round_rect(LIST_X - 20, (int)m->highlightY - 12, LIST_W, 70, 16, 0xe6f1ff, 34);
    for (int row = 0; row < BTN_ROWS; row++) {
        int i = m->buttonTop + row;
        int y = BTN_TOP + row * BTN_STEP;
        if (i > count) {
            break;
        }
        int sel = i == m->buttonSel;
        uint32_t colour = sel ? TEXT_BRIGHT : TEXT_NORMAL;
        if (i == count) {
            draw_icon(ICON_RESTART, LIST_X + 40, y + 24, 34, colour, 255);
            draw_text(FONT_MEDIUM, LIST_X + 100, y, "Reset to default", colour, 255);
        } else {
            draw_text(FONT_MEDIUM, LIST_X + 40, y, host->button_name(i), colour, 255);
            draw_value(y + 6, keyNames[key_index(host->get_button_key(i))], sel);
        }
    }
    if (m->buttonTop > 0) {
        draw_text_centered(FONT_SMALL, LIST_X + LIST_W / 2 - 20, BTN_TOP - 50, "More above",
                           TEXT_DIM, 200);
    }
    if (m->buttonTop + BTN_ROWS <= count) {
        draw_text_centered(FONT_SMALL, LIST_X + LIST_W / 2 - 20, BTN_TOP + BTN_ROWS * BTN_STEP,
                           "More below", TEXT_DIM, 200);
    }
}

static void draw_menu(const menu_state* m, const psn_menu_host* host, float t) {
    char clock[16];
    time_t now = time(NULL);
    struct tm tmv;
    draw_background(t);
    draw_text(FONT_SMALL, 80, 60, "PSNokia", TEXT_NORMAL, 255);
    if (localtime_r(&now, &tmv) != NULL) {
        snprintf(clock, sizeof(clock), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        draw_text_right(FONT_SMALL, fbW - 80, 60, clock, TEXT_NORMAL, 255);
    }
    // The game
    int cx = 440;
    if (m->inGame && frameCopy != NULL) {
        draw_game_frame(host, cx, 440, 520, 440);
    } else {
        draw_game_icon(cx, 430, 300);
    }
    draw_text_centered(FONT_LARGE, cx, 700, host->title ? host->title : "", TEXT_BRIGHT, 255);
    draw_text_centered(FONT_SMALL, cx, 770, host->subtitle ? host->subtitle : "", TEXT_DIM, 255);

    if (m->screen == 0) {
        draw_main(m, host);
    } else {
        draw_buttons(m, host);
    }
    // Hints
    int x = fbW - 80;
    const char* back = m->screen == 0 ? (m->inGame ? "Back to game" : "") : "Back";
    if (*back) {
        x -= text_width(FONT_SMALL, back);
        draw_text(FONT_SMALL, x, fbH - 90, back, TEXT_NORMAL, 255);
        draw_icon(ICON_CIRCLE, x - 30, fbH - 72, 36, TEXT_NORMAL, 255);
        x -= 90;
    }
    const char* select = m->screen == 1 ? "Change" : "Select";
    x -= text_width(FONT_SMALL, select);
    draw_text(FONT_SMALL, x, fbH - 90, select, TEXT_NORMAL, 255);
    draw_icon(ICON_CROSS, x - 30, fbH - 72, 36, TEXT_NORMAL, 255);
}

// --- Input ----------------------------------------------------------------------------

enum { IN_NONE, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK };

// DS4 button numbers in the OpenOrbis SDL2 joystick
#define DS4_CROSS 0
#define DS4_CIRCLE 1
#define DS4_OPTIONS 9
#define DS4_UP 13
#define DS4_DOWN 14
#define DS4_LEFT 15
#define DS4_RIGHT 16

static int heldDir;             // direction held, for auto-repeat
static Uint32 repeatAt;

// The next input, with held directions repeating
static int read_input(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_JOYBUTTONDOWN) {
            switch (e.jbutton.button) {
            case DS4_UP: heldDir = IN_UP; break;
            case DS4_DOWN: heldDir = IN_DOWN; break;
            case DS4_LEFT: heldDir = IN_LEFT; break;
            case DS4_RIGHT: heldDir = IN_RIGHT; break;
            case DS4_CROSS: return IN_OK;
            case DS4_CIRCLE: return IN_BACK;
            default: continue;
            }
            repeatAt = SDL_GetTicks() + 380;
            return heldDir;
        }
        if (e.type == SDL_JOYBUTTONUP) {
            int b = e.jbutton.button;
            if ((b == DS4_UP && heldDir == IN_UP) || (b == DS4_DOWN && heldDir == IN_DOWN)
                    || (b == DS4_LEFT && heldDir == IN_LEFT)
                    || (b == DS4_RIGHT && heldDir == IN_RIGHT)) {
                heldDir = IN_NONE;
            }
        }
        if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis <= 1) {
            int v = e.jaxis.value;
            int dir = IN_NONE;
            if (e.jaxis.axis == 1) {
                dir = v < -20000 ? IN_UP : v > 20000 ? IN_DOWN : IN_NONE;
            } else {
                dir = v < -20000 ? IN_LEFT : v > 20000 ? IN_RIGHT : IN_NONE;
            }
            int axisDirs = e.jaxis.axis == 1 ? (heldDir == IN_UP || heldDir == IN_DOWN)
                                             : (heldDir == IN_LEFT || heldDir == IN_RIGHT);
            if (dir == IN_NONE) {
                if (axisDirs) {
                    heldDir = IN_NONE;
                }
            } else if (dir != heldDir) {
                heldDir = dir;
                repeatAt = SDL_GetTicks() + 380;
                return dir;
            }
        }
    }
    if (heldDir != IN_NONE && SDL_GetTicks() >= repeatAt) {
        repeatAt = SDL_GetTicks() + 110;
        return heldDir;
    }
    return IN_NONE;
}

// Moves to the next enabled item in the main list
static int step_item(const menu_state* m, int from, int delta) {
    int i = from;
    do {
        i = (i + delta + ITEM_COUNT) % ITEM_COUNT;
    } while (!item_enabled(m, i) && i != from);
    return i;
}

// Returns -1 to stay in the menu, else a PSN_MENU_ action
static int handle_input(menu_state* m, const psn_menu_host* host, int in) {
    if (m->screen == 1) {
        int count = host->button_count();
        if (in == IN_UP || in == IN_DOWN) {
            m->buttonSel = (m->buttonSel + (in == IN_UP ? count : 1)) % (count + 1);
            if (m->buttonSel < m->buttonTop) m->buttonTop = m->buttonSel;
            if (m->buttonSel >= m->buttonTop + BTN_ROWS) m->buttonTop = m->buttonSel - BTN_ROWS + 1;
        } else if ((in == IN_LEFT || in == IN_RIGHT || in == IN_OK) && m->buttonSel < count) {
            int k = key_index(host->get_button_key(m->buttonSel));
            k = (k + (in == IN_LEFT ? KEY_CHOICES - 1 : 1)) % KEY_CHOICES;
            host->set_button_key(m->buttonSel, keyCodes[k]);
        } else if (in == IN_OK && m->buttonSel == count) {
            host->reset_buttons();
        } else if (in == IN_BACK) {
            host->save_buttons();
            m->screen = 0;
        }
        return -1;
    }
    if (in != IN_OK) {
        m->confirmClose = 0;
    }
    switch (in) {
    case IN_UP:
        m->selected = step_item(m, m->selected, -1);
        break;
    case IN_DOWN:
        m->selected = step_item(m, m->selected, 1);
        break;
    case IN_LEFT:
    case IN_RIGHT:
        if (m->selected == ITEM_VIEW) {
            int n = host->view_count();
            host->set_view((host->get_view() + (in == IN_LEFT ? n - 1 : 1)) % n);
        } else if (m->selected == ITEM_PICTURE) {
            host->set_smooth(!host->get_smooth());
        }
        break;
    case IN_OK:
        switch (m->selected) {
        case ITEM_START:
            return PSN_MENU_RESUME;
        case ITEM_RESTART:
            return PSN_MENU_RESTART;
        case ITEM_BUTTONS:
            m->screen = 1;
            m->buttonSel = 0;
            m->buttonTop = 0;
            m->highlightY = (float)BTN_TOP;
            break;
        case ITEM_VIEW:
            host->set_view((host->get_view() + 1) % host->view_count());
            break;
        case ITEM_PICTURE:
            host->set_smooth(!host->get_smooth());
            break;
        case ITEM_CLOSE:
            if (m->confirmClose) {
                return PSN_MENU_CLOSE;
            }
            m->confirmClose = 1;
            break;
        }
        break;
    case IN_BACK:
        if (m->inGame) {
            return PSN_MENU_RESUME;
        }
        break;
    }
    return -1;
}

// --- Entry point ------------------------------------------------------------------------

int psn_menu_run(SDL_Window* window, int in_game, const psn_menu_host* host) {
    static int initialised;
    SDL_Surface* screen = SDL_GetWindowSurface(window);
    if (screen == NULL) {
        return PSN_MENU_RESUME;
    }
    if (!initialised) {
        fbW = screen->w;
        fbH = screen->h;
        fb = (uint32_t*)malloc(sizeof(uint32_t) * fbW * fbH);
        make_background();
        load_fonts();
        load_game_icon();
        initialised = 1;
        menu_log(haveFont ? "MENU: ready\n" : "MENU: ready without text\n");
    }
    if (fb == NULL) {
        return PSN_MENU_RESUME;
    }
    SDL_Surface* fbSurface = SDL_CreateRGBSurfaceFrom(fb, fbW, fbH, 32, fbW * 4,
                                                      0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    if (host->game_frame != NULL && in_game) {
        SDL_Surface* frame = host->game_frame();
        frameCopy = frame ? SDL_ConvertSurfaceFormat(frame, SDL_PIXELFORMAT_RGB888, 0) : NULL;
    }
    menu_state m;
    memset(&m, 0, sizeof(m));
    m.inGame = in_game;
    m.highlightY = (float)MAIN_TOP;
    heldDir = IN_NONE;
    // Buttons still held from opening the menu are not a choice
    SDL_Delay(150);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    Uint32 start = SDL_GetTicks();
    int result = -1;
    while (result < 0) {
        Uint32 frameStart = SDL_GetTicks();
        int in;
        while ((in = read_input()) != IN_NONE && result < 0) {
            result = handle_input(&m, host, in);
        }
        float target = m.screen == 0 ? (float)(MAIN_TOP + m.selected * MAIN_STEP)
                                     : (float)(BTN_TOP + (m.buttonSel - m.buttonTop) * BTN_STEP);
        m.highlightY += (target - m.highlightY) * 0.35f;
        draw_menu(&m, host, (SDL_GetTicks() - start) / 1000.0f);
        if (fbSurface != NULL) {
            SDL_BlitSurface(fbSurface, NULL, screen, NULL);
        }
        SDL_UpdateWindowSurface(window);
        Uint32 spent = SDL_GetTicks() - frameStart;
        if (spent < 16) {
            SDL_Delay(16 - spent);
        }
    }
    if (fbSurface != NULL) {
        SDL_FreeSurface(fbSurface);
    }
    if (frameCopy != NULL) {
        SDL_FreeSurface(frameCopy);
        frameCopy = NULL;
    }
    heldDir = IN_NONE;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    return result;
}
