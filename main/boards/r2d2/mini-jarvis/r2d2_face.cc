#include "r2d2_face.h"

#include <esp_random.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace r2d2 {

namespace {

// face.py geometry, base units (scale 1 ≈ a 160 px tall face).
constexpr float kEyeW = 74, kEyeH = 68, kEyeGap = 36, kStroke = 6;
constexpr float kEyeY = -25, kMouthY = 63;
constexpr float kBoxTop = -95, kBoxBottom = 110;  // face.py _BOX, vertical extent
constexpr float kDegToRad = 3.14159265f / 180.0f;

const char* kEmotions[] = {"neutral", "confident", "curious", "surprised", "suspicious", "happy"};

struct Pool {
    const char* state;
    const char* hint;  // nullptr: the state's default pool
    std::vector<const char*> variants;
};

const std::vector<Pool>& Pools() {
    static const std::vector<Pool> pools = {
        {"listening", nullptr, {"default", "focus", "blink", "left", "right"}},
        {"listening", "sound", {"sound", "loud", "focus"}},
        {"thinking", nullptr, {"default", "left", "right", "up", "down", "focused", "pause"}},
        {"speaking", nullptr, {"talk1", "talk2", "talk3", "talk4", "talk5"}},
    };
    return pools;
}

const std::vector<const char*>* FindPool(const std::string& state, const std::string& hint) {
    for (const auto& pool : Pools()) {
        if (state == pool.state && pool.hint != nullptr && hint == pool.hint) return &pool.variants;
    }
    for (const auto& pool : Pools()) {
        if (state == pool.state && pool.hint == nullptr) return &pool.variants;
    }
    return nullptr;
}

bool In(const std::string& v, std::initializer_list<const char*> options) {
    for (auto o : options) {
        if (v == o) return true;
    }
    return false;
}

// ── face.py drawing helpers ──────────────────────────────────────────────────

void OpenEye(Pen& p, float cx, float cy, float px = 0, float py = 0, float scale = 1) {
    float w = kEyeW * scale, h = kEyeH * scale;
    p.Ring(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
    p.Dot(cx + px * w * 0.22f, cy + py * h * 0.20f, std::max(6.0f, std::min(w, h) * 0.13f));
}

void HappyEye(Pen& p, float cx, float cy) {
    p.Arc(cx - kEyeW / 2, cy - kEyeH / 3, cx + kEyeW / 2, cy + kEyeH / 2, 195, 345);
}

void SleepEye(Pen& p, float cx, float cy) {
    float w = kEyeW * 0.78f;
    p.Arc(cx - w / 2, cy - 13, cx + w / 2, cy + 21, 25, 155);
}

float BrowWidth(Pen& p) { return std::max(2.0f, std::round((kStroke - 1) * p.k())); }

void Brow(Pen& p, float cx, float cy, const char* kind) {
    float w = kEyeW * 0.58f;
    float y = cy - kEyeH * 0.68f;
    float bw = BrowWidth(p);
    std::string k = kind;
    if (k == "normal" || k == "raised") {
        float lift = k == "raised" ? 7 : 0;
        p.Arc(cx - w / 2, y - 4 - lift, cx + w / 2, y + 14 - lift, 205, 335, bw);
    } else if (k == "in") {
        p.Line2(cx - w / 2, y - 3, cx + w / 2, y + 7, bw);
    } else if (k == "out") {
        p.Line2(cx - w / 2, y + 7, cx + w / 2, y - 3, bw);
    } else {  // flat
        p.Line2(cx - w / 2, y, cx + w / 2, y, bw);
    }
}

void Mouth(Pen& p, float cx, float cy, const char* kind) {
    std::string k = kind;
    if (k == "flat") {
        p.Line2(cx - 30, cy, cx + 30, cy);
    } else if (k == "short") {
        p.Line2(cx - 17, cy, cx + 17, cy);
    } else if (k == "tiny") {
        p.Line2(cx - 9, cy, cx + 9, cy);
    } else if (k == "open_small") {
        p.Ring(cx - 13, cy - 8, cx + 13, cy + 8);
    } else if (k == "open") {
        p.Ring(cx - 18, cy - 11, cx + 18, cy + 11);
    } else if (k == "wide") {
        p.Ring(cx - 27, cy - 9, cx + 27, cy + 9);
    } else if (k == "smile") {
        p.Arc(cx - 31, cy - 21, cx + 31, cy + 22, 20, 160);
    } else if (k == "frown") {
        p.Arc(cx - 31, cy - 1, cx + 31, cy + 39, 200, 340);
    } else if (k == "wave") {
        float pts[26];
        int n = 0;
        for (int i = -30; i <= 30; i += 5) {
            pts[n++] = cx + i;
            pts[n++] = cy + std::sin(i / 8.0f) * 5;
        }
        p.Line(pts, n / 2);
    } else if (k == "crooked") {
        float pts[] = {cx - 26, cy + 3, cx - 6, cy - 1, cx + 24, cy + 4};
        p.Line(pts, 3);
    }
}

void SoundWaves(Pen& p, float cx, float cy, int strength) {
    float w = BrowWidth(p);
    for (int side : {-1, 1}) {
        float x = cx + side * 110;
        for (int i = 0; i < std::max(1, strength); i++) {
            float pad = i * 12;
            if (side < 0) {
                p.Arc(x - 24 - pad, cy - 33 - pad, x + 15, cy + 33 + pad, 105, 255, w);
            } else {
                p.Arc(x - 15, cy - 33 - pad, x + 24 + pad, cy + 33 + pad, 285, 75, w);
            }
        }
    }
}

void Dots(Pen& p, float x, float y, int count = 3) {
    for (int i = 0; i < count; i++) p.Dot(x + i * 24, y, 5);
}

void CrossEye(Pen& p, float cx, float cy) {
    float r = 25;
    p.Line2(cx - r, cy - r, cx + r, cy + r);
    p.Line2(cx + r, cy - r, cx - r, cy + r);
}

void DrawExpression(Pen& p, const std::string& state, const std::string& full_variant) {
    const float lx = -(kEyeW + kEyeGap) / 2, rx = (kEyeW + kEyeGap) / 2;
    const float ey = kEyeY, my = kMouthY;

    auto eyes = [&](float p1x = 0, float p1y = 0, float p2x = 0, float p2y = 0, float scale = 1) {
        OpenEye(p, lx, ey, p1x, p1y, scale);
        OpenEye(p, rx, ey, p2x, p2y, scale);
    };
    auto brows = [&](const char* left, const char* right = nullptr) {
        Brow(p, lx, ey, left);
        Brow(p, rx, ey, right ? right : left);
    };
    auto emotion_eyes = [&](const std::string& e) {
        if (e == "greeting" || e == "happy") {
            HappyEye(p, lx, ey);
            HappyEye(p, rx, ey);
        } else if (e == "confident") {
            eyes();
            brows("raised");
        } else if (e == "curious") {
            eyes(.35f, -.15f, .35f, -.15f);
            brows("raised", "flat");
        } else if (e == "surprised") {
            eyes(0, 0, 0, 0, 1.05f);
            brows("raised");
        } else if (e == "suspicious") {
            eyes(.35f, 0, -.35f, 0, .88f);
            brows("in", "out");
        } else {  // neutral
            eyes();
            brows("normal");
        }
    };

    const std::string& v = full_variant;
    if (state == "listening") {
        if (v == "blink") {
            SleepEye(p, lx, ey);
            SleepEye(p, rx, ey);
            Mouth(p, 0, my, "flat");
            return;
        }
        if (v == "left") eyes(-.75f, 0, -.75f, 0);
        else if (v == "right") eyes(.75f, 0, .75f, 0);
        else if (v == "sound") eyes(.15f, 0, -.15f, 0);
        else eyes(0, 0, 0, 0, v == "focus" ? .90f : 1.0f);
        brows(In(v, {"sound", "loud"}) ? "raised" : "normal");
        Mouth(p, 0, my, v == "sound" ? "short" : "flat");
        if (In(v, {"sound", "loud"})) SoundWaves(p, 0, ey, v == "loud" ? 3 : 2);

    } else if (state == "thinking") {
        if (v == "pause") {
            SleepEye(p, lx, ey);
            SleepEye(p, rx, ey);
            Mouth(p, 0, my, "tiny");
            Dots(p, rx + 65, ey + 15);
            return;
        }
        if (v == "left") eyes(-.72f, -.05f, -.72f, -.05f);
        else if (v == "right") eyes(.72f, -.05f, .72f, -.05f);
        else if (v == "up") eyes(0, -.75f, 0, -.75f);
        else if (v == "down") eyes(0, .72f, 0, .72f);
        else if (v == "focused") eyes(0, .15f, 0, .15f, .88f);
        else eyes(-.28f, -.22f, -.28f, -.22f);
        if (v == "focused") brows("in", "out");
        else if (v == "up") brows("raised");
        else brows("normal", "flat");
        Mouth(p, 0, my, "short");

    } else if (state == "speaking") {
        auto plus = v.rfind('+');
        std::string emotion = plus == std::string::npos ? "" : v.substr(0, plus);
        std::string talk = plus == std::string::npos ? v : v.substr(plus + 1);
        if (!emotion.empty()) {
            emotion_eyes(emotion);
        } else {
            HappyEye(p, lx, ey);
            HappyEye(p, rx, ey);
        }
        const char* mouth = "open_small";
        if (talk == "talk1") mouth = "short";
        else if (talk == "talk3") mouth = "wide";
        else if (talk == "talk4") mouth = "flat";
        else if (talk == "talk5") mouth = "open";
        else if (talk == "finish") mouth = "smile";
        Mouth(p, 0, my, mouth);
        if (In(talk, {"talk3", "talk5"})) SoundWaves(p, 0, ey, 1);

    } else if (state == "error") {
        if (v == "critical") {
            CrossEye(p, lx, ey);
            CrossEye(p, rx, ey);
            Mouth(p, 0, my, "frown");
        } else if (v == "connection") {
            OpenEye(p, lx, ey, -.2f, 0);
            p.Line2(rx - 24, ey, rx + 24, ey);
            Brow(p, lx, ey, "in");
            Mouth(p, 0, my, "frown");
        } else if (v == "timeout") {
            eyes(0, .25f, 0, .25f, .85f);
            brows("flat");
            Mouth(p, 0, my, "flat");
        } else if (v == "permission") {
            eyes(-.35f, 0, .35f, 0);
            brows("in", "out");
            Mouth(p, 0, my, "flat");
        } else if (v == "confused") {
            eyes(-.45f, -.2f, .35f, .25f);
            brows("flat", "raised");
            Mouth(p, 0, my, "crooked");
        } else {  // default / service
            eyes(-.15f, .05f, .15f, .05f);
            brows("in", "out");
            Mouth(p, 0, my, v == "service" ? "wave" : "frown");
        }

    } else if (state == "extra") {
        emotion_eyes(v);
        const char* mouth = "flat";
        if (In(v, {"greeting", "happy", "confident"})) mouth = "smile";
        else if (v == "curious") mouth = "open_small";
        else if (v == "surprised") mouth = "open";
        Mouth(p, 0, my, mouth);
    }
}

lv_draw_buf_t* MakeCanvas(lv_obj_t* parent, lv_obj_t** canvas, int w, int h) {
    lv_draw_buf_t* buf = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    *canvas = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(*canvas, buf);
    lv_canvas_fill_bg(*canvas, Rgb(kBg), LV_OPA_COVER);
    return buf;
}

}  // namespace

// ── Pen ──────────────────────────────────────────────────────────────────────

Pen::Pen(lv_layer_t* layer, float ox, float oy, float k, lv_color_t color, lv_opa_t opa, float width_factor)
    : layer_(layer), ox_(ox), oy_(oy), k_(k), color_(color), opa_(opa), width_factor_(width_factor) {}

void Pen::Line(const float* xy, int points, float width) {
    float w = (width > 0 ? width : std::max(2.0f, std::round(kStroke * k_))) * width_factor_;
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color_;
    dsc.opa = opa_;
    dsc.width = std::max<int32_t>(1, std::lround(w));
    dsc.round_start = 1;
    dsc.round_end = 1;
    for (int i = 0; i + 1 < points; i++) {
        dsc.p1.x = (lv_value_precise_t)(ox_ + xy[2 * i] * k_);
        dsc.p1.y = (lv_value_precise_t)(oy_ + xy[2 * i + 1] * k_);
        dsc.p2.x = (lv_value_precise_t)(ox_ + xy[2 * i + 2] * k_);
        dsc.p2.y = (lv_value_precise_t)(oy_ + xy[2 * i + 3] * k_);
        lv_draw_line(layer_, &dsc);
    }
}

void Pen::Line2(float x0, float y0, float x1, float y1, float width) {
    float pts[] = {x0, y0, x1, y1};
    Line(pts, 2, width);
}

void Pen::Arc(float x0, float y0, float x1, float y1, float start, float end, float width) {
    if (end < start) end += 360;
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
    int steps = std::max(4, (int)std::ceil((end - start) / 10.0f));
    std::vector<float> pts;
    pts.reserve((steps + 1) * 2);
    for (int i = 0; i <= steps; i++) {
        float a = (start + (end - start) * i / steps) * kDegToRad;
        pts.push_back(cx + rx * std::cos(a));
        pts.push_back(cy + ry * std::sin(a));
    }
    Line(pts.data(), steps + 1, width);
}

void Pen::Ring(float x0, float y0, float x1, float y1) { Arc(x0, y0, x1, y1, 0, 360); }

void Pen::Dot(float x, float y, float r) {
    float rr = r * k_ + (width_factor_ - 1) * kStroke * k_ / 2;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = color_;
    dsc.bg_opa = opa_;
    dsc.border_width = 0;
    lv_area_t area = {(int32_t)std::lround(ox_ + x * k_ - rr), (int32_t)std::lround(oy_ + y * k_ - rr),
                      (int32_t)std::lround(ox_ + x * k_ + rr), (int32_t)std::lround(oy_ + y * k_ + rr)};
    lv_draw_rect(layer_, &dsc, &area);
}

// ── face catalog ─────────────────────────────────────────────────────────────

lv_color_t FaceColor(const std::string& state) {
    if (state == "listening") return Rgb(kVoiceListen);
    if (state == "thinking") return Rgb(kVoiceThink);
    if (state == "speaking") return Rgb(kVoiceSpeak);
    if (state == "error") return Rgb(kVoiceError);
    if (state == "extra") return Rgb(kVoiceExtra);
    return Rgb(kTextDim);
}

const char* FaceLabel(const std::string& state, const std::string& variant) {
    if (state == "extra") {
        if (variant == "greeting") return "OLÁ";
        if (variant == "happy") return "ATÉ MAIS";
        if (variant == "confident") return "PRONTO";
        return "";
    }
    if (state == "error") {
        if (variant == "connection") return "SEM CONEXÃO";
        if (variant == "service") return "SERVIÇO INDISPONÍVEL";
        if (variant == "timeout") return "TEMPO ESGOTADO";
        if (variant == "permission") return "ACESSO NEGADO";
        if (variant == "critical") return "ERRO GRAVE";
        if (variant == "confused") return "OCUPADO";
        return "ERRO";
    }
    if (state == "listening") return "OUVINDO";
    if (state == "thinking") return "PENSANDO";
    if (state == "speaking") return "FALANDO";
    return "";
}

bool IsEmotion(const std::string& name) {
    for (auto e : kEmotions) {
        if (name == e) return true;
    }
    return false;
}

std::string PickVariant(const std::string& state, const std::string& hint, const std::string& previous) {
    if (state == "speaking" && IsEmotion(hint)) {
        auto plus = previous.rfind('+');
        std::string prev_talk = plus == std::string::npos ? previous : previous.substr(plus + 1);
        return hint + "+" + PickVariant("speaking", "", prev_talk);
    }
    const auto* pool = FindPool(state, hint);
    if (pool == nullptr) return hint;  // error / extra: the variant is the message
    for (int tries = 0; tries < 8; tries++) {
        std::string v = (*pool)[esp_random() % pool->size()];
        if (v != previous || pool->size() == 1) return v;
    }
    return (*pool)[0];
}

// ── Face ─────────────────────────────────────────────────────────────────────

Face::Face(lv_obj_t* parent, int w, int h, float scale) : w_(w), h_(h), scale_(scale) {
    buf_ = MakeCanvas(parent, &canvas_, w, h);
}

Face::~Face() {
    if (canvas_) lv_obj_delete(canvas_);
    if (buf_) lv_draw_buf_destroy(buf_);
}

void Face::Draw(const std::string& state, const std::string& variant) {
    lv_canvas_fill_bg(canvas_, Rgb(kBg), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    float ox = w_ / 2.0f;
    float oy = h_ / 2.0f - (kBoxTop + kBoxBottom) / 2 * scale_;
    lv_color_t color = FaceColor(state);
    // Halo first (the HUD blurs a mask; a wide faint stroke reads the same at this size).
    Pen glow(&layer, ox, oy, scale_, color, LV_OPA_20, 2.6f);
    DrawExpression(glow, state, variant);
    Pen pen(&layer, ox, oy, scale_, color);
    DrawExpression(pen, state, variant);
    lv_canvas_finish_layer(canvas_, &layer);
}

// ── WeatherIcon ──────────────────────────────────────────────────────────────

WeatherIcon::WeatherIcon(lv_obj_t* parent, int size) : size_(size) {
    buf_ = MakeCanvas(parent, &canvas_, size, size);
}

WeatherIcon::~WeatherIcon() {
    if (canvas_) lv_obj_delete(canvas_);
    if (buf_) lv_draw_buf_destroy(buf_);
}

void WeatherIcon::Draw(const std::string& icon, bool night) {
    lv_canvas_fill_bg(canvas_, Rgb(kBg), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    // Base box ±28 units; stroke kStroke·k like the face.
    float k = size_ / 60.0f, o = size_ / 2.0f;
    auto cloud = [](Pen& p, float dy) {
        p.Arc(-24, -6 + dy, -4, 14 + dy, 90, 270);      // left bump
        p.Arc(-14, -20 + dy, 14, 8 + dy, 180, 360);     // top bump
        p.Arc(2, -10 + dy, 24, 14 + dy, 270, 450);      // right bump
        p.Line2(-14, 14 + dy, 13, 14 + dy);             // base
    };
    if (icon == "clear" && !night) {
        Pen p(&layer, o, o, k, Rgb(kAccent));
        p.Ring(-11, -11, 11, 11);
        for (int i = 0; i < 8; i++) {
            float a = i * 45 * kDegToRad;
            p.Line2(std::cos(a) * 17, std::sin(a) * 17, std::cos(a) * 24, std::sin(a) * 24);
        }
    } else if (icon == "clear") {
        Pen p(&layer, o, o, k, Rgb(kTextDim));
        p.Arc(-16, -16, 16, 16, 50, 310);   // moon: outer edge
        p.Arc(-6, -16, 26, 16, 115, 245);   // inner edge of the crescent
    } else if (icon == "rain") {
        Pen p(&layer, o, o, k, Rgb(kRainTint));
        cloud(p, -6);
        p.Line2(-10, 14, -14, 24);
        p.Line2(0, 14, -4, 24);
        p.Line2(10, 14, 6, 24);
    } else if (icon == "storm") {
        Pen p(&layer, o, o, k, Rgb(kStormTint));
        cloud(p, -6);
        float bolt[] = {4, 10, -4, 18, 3, 18, -4, 27};
        p.Line(bolt, 4);
    } else if (icon == "fog") {
        Pen p(&layer, o, o, k, Rgb(kTextDim));
        p.Line2(-20, -8, 20, -8);
        p.Line2(-24, 2, 16, 2);
        p.Line2(-16, 12, 24, 12);
    } else {
        Pen p(&layer, o, o, k, Rgb(kTextDim));
        cloud(p, 0);
    }
    lv_canvas_finish_layer(canvas_, &layer);
}

}  // namespace r2d2
