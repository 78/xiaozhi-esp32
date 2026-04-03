#include "animated_eyes.h"
#include <cstdlib>
#include <cmath>
#include <cstring>

// Helper: set pixel in 1bpp buffer using stride-based row addressing
// stride = bytes per row (from lv_draw_buf), w = pixel width for bounds check
static inline void set_pixel(uint8_t* buf, int w, int stride, int x, int y, bool on) {
    if (x < 0 || x >= w || y < 0) return;
    int byte_idx = y * stride + x / 8;
    int bit_idx = 7 - (x % 8);
    if (on)
        buf[byte_idx] |= (1 << bit_idx);
    else
        buf[byte_idx] &= ~(1 << bit_idx);
}

// Draw a filled ellipse (approximate with scanlines)
static void draw_filled_ellipse(uint8_t* buf, int bw, int stride, int bh,
                                 int cx, int cy, int rx, int ry, bool on) {
    if (rx <= 0 || ry <= 0) return;
    for (int dy = -ry; dy <= ry; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= bh) continue;
        int dx_max = (int)(rx * sqrtf(1.0f - (float)(dy * dy) / (float)(ry * ry)));
        for (int dx = -dx_max; dx <= dx_max; dx++) {
            int x = cx + dx;
            set_pixel(buf, bw, stride, x, y, on);
        }
    }
}

// Draw a horizontal line
static void draw_hline(uint8_t* buf, int bw, int stride, int x1, int x2, int y, bool on) {
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    for (int x = x1; x <= x2; x++) {
        set_pixel(buf, bw, stride, x, y, on);
    }
}


// Draw a single eye with brow/lid
static void draw_eye(uint8_t* buf, int bw, int stride, int bh, const EyeState* eye) {
    if (!eye->visible) return;

    int cx = eye->cx;
    int cy = eye->cy;
    int w = eye->width;
    int h = eye->height;

    // Skip if eye is fully closed
    if (eye->lid_top >= h && eye->lid_bot >= h) return;

    // Effective visible height after lids
    int vis_top = cy - h + eye->lid_top;
    int vis_bot = cy + h - eye->lid_bot;

    if (vis_top >= vis_bot) return;

    // Draw eye outline (ellipse shape, but clipped by lids)
    for (int dy = -h; dy <= h; dy++) {
        int y = cy + dy;
        if (y < vis_top || y > vis_bot) continue;
        if (y < 0 || y >= bh) continue;

        int dx_max = (int)(w * sqrtf(1.0f - (float)(dy * dy) / (float)(h * h)));
        // Draw outline pixels
        set_pixel(buf, bw, stride, cx - dx_max, y, true);
        set_pixel(buf, bw, stride, cx + dx_max, y, true);
        if (dx_max > 0) {
            set_pixel(buf, bw, stride, cx - dx_max + 1, y, true);
            set_pixel(buf, bw, stride, cx + dx_max - 1, y, true);
        }
    }

    // Draw top lid line
    {
        int lid_y = vis_top;
        if (lid_y >= 0 && lid_y < bh) {
            int dy = lid_y - cy;
            float frac = (float)(dy * dy) / (float)(h * h);
            if (frac <= 1.0f) {
                int dx_max = (int)(w * sqrtf(1.0f - frac));
                // Draw lid with brow angle
                for (int dx = -dx_max; dx <= dx_max; dx++) {
                    int angle_offset = (eye->brow_angle * dx) / (w > 0 ? w : 1) / 3;
                    int y = lid_y + angle_offset;
                    set_pixel(buf, bw, stride, cx + dx, y, true);
                    if (y + 1 < bh) set_pixel(buf, bw, stride, cx + dx, y + 1, true);
                }
            }
        }
    }

    // Draw bottom lid line
    {
        int lid_y = vis_bot;
        if (lid_y >= 0 && lid_y < bh) {
            int dy = lid_y - cy;
            float frac = (float)(dy * dy) / (float)(h * h);
            if (frac <= 1.0f) {
                int dx_max = (int)(w * sqrtf(1.0f - frac));
                draw_hline(buf, bw, stride, cx - dx_max, cx + dx_max, lid_y, true);
            }
        }
    }

    // Draw pupil (filled small circle)
    int px = cx + eye->pupil_dx;
    int py = cy + eye->pupil_dy;
    int pr = eye->pupil_r;
    // Clamp pupil within visible area
    if (py - pr < vis_top) py = vis_top + pr;
    if (py + pr > vis_bot) py = vis_bot - pr;
    draw_filled_ellipse(buf, bw, stride, bh, px, py, pr, pr, true);
}

// Draw mouth shape
// height > 0 = smile (curved up), height < 0 = frown (curved down), 0 = flat
// open > 0 = open mouth (filled ellipse below the curve)
static void draw_mouth(uint8_t* buf, int bw, int stride, int bh, const MouthState* mouth) {
    if (!mouth->visible) return;

    int cx = mouth->cx;
    int cy = mouth->cy;
    int mw = mouth->width;
    int mh = mouth->height;  // curvature: positive=smile, negative=frown
    int open = mouth->open;

    if (mw <= 0) return;

    // Draw the mouth curve (quadratic bezier-like)
    // For each x from -mw to +mw, compute y offset based on curvature
    for (int dx = -mw; dx <= mw; dx++) {
        float t = (float)dx / (float)mw;  // -1 to 1
        // Parabolic curve: y_offset = -mh * (1 - t^2) for smile
        // At edges (t=+-1): y_offset = 0, at center (t=0): y_offset = -mh
        int y_offset = (int)(-mh * (1.0f - t * t));
        int y = cy + y_offset;
        int x = cx + dx;
        set_pixel(buf, bw, stride, x, y, true);
        // Thicken the line
        if (y + 1 < bh) set_pixel(buf, bw, stride, x, y + 1, true);
    }

    // If mouth is open, fill the area between the curve and a mirrored curve below
    if (open > 0) {
        for (int dx = -mw; dx <= mw; dx++) {
            float t = (float)dx / (float)mw;
            int top_offset = (int)(-mh * (1.0f - t * t));
            int top_y = cy + top_offset;
            // Bottom curve: mirror with open amount
            int bot_y = top_y + open;
            // Clamp
            if (top_y < 0) top_y = 0;
            if (bot_y >= bh) bot_y = bh - 1;
            int x = cx + dx;
            for (int y = top_y + 1; y <= bot_y; y++) {
                set_pixel(buf, bw, stride, x, y, true);
            }
        }
    }
}

// ---- Emotion presets ----
// Eyes shifted up to Y=22, mouth at Y=52
// Left eye center ~(32, 22), Right eye center ~(96, 22)

#define LE_CX 32
#define LE_CY 22
#define RE_CX 96
#define RE_CY 22
#define MOUTH_CX 64
#define MOUTH_CY 52

// Macro: symmetric robot eyes + mouth
// mouth args: mw=half-width, mh=curvature(+smile/-frown), mo=open amount
#define SYM_FACE(w, h, pdx, pdy, pr, lt, lb, ba, mw, mh, mo) \
    .right_eye = {RE_CX, RE_CY, w, h, pdx, pdy, pr, lt, lb, -(ba), true}, \
    .left_eye  = {LE_CX, LE_CY, w, h, -(pdx), pdy, pr, lt, lb, ba, true}, \
    .left_eye_open = false, \
    .mouth = {MOUTH_CX, MOUTH_CY, mw, mh, mo, true}

static const FaceState emotion_presets[EMOTION_COUNT] = {
    // NEUTRAL - round open eyes, slight smile
    { SYM_FACE(18, 16, 0, 0, 5, 2, 2, 0,   16, 2, 0) },
    // HAPPY - squinted eyes, wide smile
    { SYM_FACE(20, 11, 0, 2, 4, 5, 4, 0,   20, 6, 3) },
    // SAD - droopy eyes, frown
    { SYM_FACE(16, 14, 0, 3, 5, 4, 0, 4,   14, -4, 0) },
    // ANGRY - narrow eyes, tight frown
    { SYM_FACE(20, 11, 0, 0, 5, 2, 3, -6,  16, -3, 0) },
    // SURPRISED - wide eyes, open O mouth
    { SYM_FACE(22, 20, 0, -1, 6, 0, 0, 2,  8, 0, 8) },
    // DISGUSTED - squinting, wavy frown
    { SYM_FACE(18, 9, -1, 1, 4, 5, 3, -4,  14, -3, 2) },
    // FEARFUL - wide tense eyes, small open mouth
    { SYM_FACE(20, 18, 0, -1, 6, 0, 0, 4,  10, -2, 5) },
    // SLEEPY - heavy lids, flat mouth
    { SYM_FACE(18, 16, 0, 3, 5, 13, 2, 0,  12, 0, 0) },
    // CONFUSED - looking aside, wavy mouth
    { SYM_FACE(18, 14, 4, -2, 5, 2, 0, 3,  12, -1, 0) },
    // THINKING - looking up, small flat
    { SYM_FACE(18, 14, 5, -4, 5, 2, 2, 1,  8, 1, 0) },
    // WINK - one eye closed, smirk mouth
    {
        .right_eye = {RE_CX, RE_CY, 18, 16, 0, 0, 5, 16, 16, 0, true},
        .left_eye  = {LE_CX, LE_CY, 18, 16, 0, 0, 5, 2, 2, 0, true},
        .left_eye_open = false,
        .mouth = {MOUTH_CX + 6, MOUTH_CY, 14, 4, 0, true},
    },
    // COOL - narrow confident, smirk
    { SYM_FACE(22, 9, 1, 0, 4, 4, 3, -3,   18, 3, 0) },
    // LAUGHING - squinted, wide open smile
    { SYM_FACE(20, 7, 0, 2, 3, 6, 5, 0,    22, 5, 5) },
    // SHY - small looking down, tiny smile
    { SYM_FACE(14, 12, -2, 3, 4, 4, 2, 2,  10, 2, 0) },
    // CRYING - sad wide, open frown
    { SYM_FACE(16, 16, 0, 4, 5, 3, 0, 5,   14, -5, 3) },
    // LOVE - soft wide, gentle smile
    { SYM_FACE(20, 16, 0, 0, 6, 2, 2, 1,   18, 5, 0) },
    // EXCITED - wide intense, big smile open
    { SYM_FACE(22, 18, 0, -1, 6, 0, 0, -3, 22, 6, 4) },
    // BORED - half closed, flat line
    { SYM_FACE(18, 16, -2, 3, 5, 12, 2, 0, 14, 0, 0) },
    // SMIRK - asymmetric eyes, crooked smile
    {
        .right_eye = {RE_CX, RE_CY, 20, 14, 2, 0, 5, 2, 2, -3, true},
        .left_eye  = {LE_CX, LE_CY, 18, 12, 0, 0, 5, 6, 4, 0, true},
        .left_eye_open = false,
        .mouth = {MOUTH_CX + 8, MOUTH_CY, 16, 4, 0, true},
    },
    // DETERMINED - intense, firm line
    { SYM_FACE(20, 12, 0, 0, 5, 1, 2, -5,  16, -1, 0) },
    // RELAXED - half-lidded, gentle smile
    { SYM_FACE(18, 14, 0, 1, 5, 8, 2, 0,   16, 3, 0) },
};

EmotionPreset emotion_string_to_preset(const char* emotion) {
    if (!emotion) return EMOTION_NEUTRAL;

    struct EmotionMap {
        const char* name;
        EmotionPreset preset;
    };

    static const EmotionMap map[] = {
        {"neutral",    EMOTION_NEUTRAL},
        {"happy",      EMOTION_HAPPY},
        {"sad",        EMOTION_SAD},
        {"angry",      EMOTION_ANGRY},
        {"surprised",  EMOTION_SURPRISED},
        {"disgusted",  EMOTION_DISGUSTED},
        {"fearful",    EMOTION_FEARFUL},
        {"sleepy",     EMOTION_SLEEPY},
        {"confused",   EMOTION_CONFUSED},
        {"thinking",   EMOTION_THINKING},
        {"wink",       EMOTION_WINK},
        {"cool",       EMOTION_COOL},
        {"laughing",   EMOTION_LAUGHING},
        {"shy",        EMOTION_SHY},
        {"crying",     EMOTION_CRYING},
        {"love",       EMOTION_LOVE},
        {"loving",     EMOTION_LOVE},
        {"excited",    EMOTION_EXCITED},
        {"bored",      EMOTION_BORED},
        {"smirk",      EMOTION_SMIRK},
        {"determined", EMOTION_DETERMINED},
        {"relaxed",    EMOTION_RELAXED},
    };

    for (const auto& entry : map) {
        if (strcmp(emotion, entry.name) == 0) {
            return entry.preset;
        }
    }
    return EMOTION_NEUTRAL;
}

const FaceState& get_emotion_face(EmotionPreset preset) {
    if (preset >= EMOTION_COUNT) preset = EMOTION_NEUTRAL;
    return emotion_presets[preset];
}

static inline int16_t lerp16(int16_t a, int16_t b, int t) {
    return (int16_t)(a + ((b - a) * t) / 256);
}

static void eye_lerp(EyeState* out, const EyeState* a, const EyeState* b, int t) {
    out->cx       = lerp16(a->cx, b->cx, t);
    out->cy       = lerp16(a->cy, b->cy, t);
    out->width    = lerp16(a->width, b->width, t);
    out->height   = lerp16(a->height, b->height, t);
    out->pupil_dx = lerp16(a->pupil_dx, b->pupil_dx, t);
    out->pupil_dy = lerp16(a->pupil_dy, b->pupil_dy, t);
    out->pupil_r  = lerp16(a->pupil_r, b->pupil_r, t);
    out->lid_top  = lerp16(a->lid_top, b->lid_top, t);
    out->lid_bot  = lerp16(a->lid_bot, b->lid_bot, t);
    out->brow_angle = lerp16(a->brow_angle, b->brow_angle, t);
    out->visible  = a->visible || b->visible;
}

static void mouth_lerp(MouthState* out, const MouthState* a, const MouthState* b, int t) {
    out->cx     = lerp16(a->cx, b->cx, t);
    out->cy     = lerp16(a->cy, b->cy, t);
    out->width  = lerp16(a->width, b->width, t);
    out->height = lerp16(a->height, b->height, t);
    out->open   = lerp16(a->open, b->open, t);
    out->visible = a->visible || b->visible;
}

void face_lerp(FaceState* out, const FaceState* from, const FaceState* to, int t) {
    eye_lerp(&out->right_eye, &from->right_eye, &to->right_eye, t);
    eye_lerp(&out->left_eye, &from->left_eye, &to->left_eye, t);
    mouth_lerp(&out->mouth, &from->mouth, &to->mouth, t);
    out->left_eye_open = (t > 128) ? to->left_eye_open : from->left_eye_open;
}

void draw_face(uint8_t* buf, int canvas_w, int canvas_h, int stride, const FaceState* face) {
    memset(buf, 0, stride * canvas_h);

    draw_eye(buf, canvas_w, stride, canvas_h, &face->left_eye);
    draw_eye(buf, canvas_w, stride, canvas_h, &face->right_eye);
    draw_mouth(buf, canvas_w, stride, canvas_h, &face->mouth);

    // Invert: SSD1306 treats 0=lit, 1=off
    int total_bytes = stride * canvas_h;
    for (int i = 0; i < total_bytes; i++) {
        buf[i] = ~buf[i];
    }
}
