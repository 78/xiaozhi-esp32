// Jarvis face for the R2D2 mini-jarvis round screen.
//
// A port of the R2D2 HUD's face (r2d2 repo, edge/dashboard/face.py): same
// geometry in the same base units, same states, variants and colors, drawn
// with LVGL primitives on a canvas instead of Pillow. Ellipses and elliptic
// arcs become short polylines, the way Pillow rasterizes them.
#pragma once

#include <lvgl.h>

#include <string>

namespace r2d2 {

// HUD colors (edge/dashboard/colors.py).
inline lv_color_t Rgb(uint32_t hex) { return lv_color_hex(hex); }
constexpr uint32_t kBg = 0x12161F;
constexpr uint32_t kText = 0xE8ECF3;
constexpr uint32_t kTextDim = 0x7C879C;
constexpr uint32_t kTextFaint = 0x4D5872;
constexpr uint32_t kAccent = 0xD9A15B;
constexpr uint32_t kRainTint = 0x5BA8D9;
constexpr uint32_t kStormTint = 0xE8C84C;
constexpr uint32_t kVoiceListen = 0x4CC3E8;
constexpr uint32_t kVoiceThink = 0xE0A83E;
constexpr uint32_t kVoiceSpeak = 0x4CE8A0;
constexpr uint32_t kVoiceError = 0xD9534F;
constexpr uint32_t kVoiceExtra = 0x91CDF0;

// Draws in the HUD's base units around (ox, oy) on a canvas layer, scaled by k.
class Pen {
public:
    Pen(lv_layer_t* layer, float ox, float oy, float k, lv_color_t color, lv_opa_t opa = LV_OPA_COVER,
        float width_factor = 1.0f);

    void Line(const float* xy, int points, float width = 0);
    void Line2(float x0, float y0, float x1, float y1, float width = 0);
    // Pillow semantics: box = (x0, y0, x1, y1), angles in degrees, clockwise from 3 o'clock.
    void Arc(float x0, float y0, float x1, float y1, float start, float end, float width = 0);
    void Ring(float x0, float y0, float x1, float y1);
    void Dot(float x, float y, float r);
    float k() const { return k_; }

private:
    lv_layer_t* layer_;
    float ox_, oy_, k_;
    lv_color_t color_;
    lv_opa_t opa_;
    float width_factor_;
};

// HUD state color: listening / thinking / speaking / error / extra.
lv_color_t FaceColor(const std::string& state);
// "OUVINDO", "PENSANDO"... or "" (face.py label()).
const char* FaceLabel(const std::string& state, const std::string& variant);
// Next variant for a state, never the previous one (face.py pools). hint: the
// agent's variant (listening "sound", error kind, extra emotion, or the
// emotion worn while speaking, which comes back as "emotion+talkN").
std::string PickVariant(const std::string& state, const std::string& hint, const std::string& previous);
// True for an emotion the voice model may pick (face.py EMOTIONS).
bool IsEmotion(const std::string& name);

class Face {
public:
    // A w×h canvas child of parent; the face (base box ≈ 380×205 units) is
    // drawn centered, scaled by `scale`.
    Face(lv_obj_t* parent, int w, int h, float scale);
    ~Face();

    // Redraw. Must be called with the display lock held (or from an LVGL timer).
    void Draw(const std::string& state, const std::string& variant);
    lv_obj_t* obj() const { return canvas_; }

private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* buf_ = nullptr;
    int w_, h_;
    float scale_;
};

// Weather glyphs in the same stroke style: "clear" (sun or, at night, moon),
// "rain", "storm", "fog", or a plain cloud for anything else.
class WeatherIcon {
public:
    WeatherIcon(lv_obj_t* parent, int size);
    ~WeatherIcon();
    void Draw(const std::string& icon, bool night);
    lv_obj_t* obj() const { return canvas_; }

private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* buf_ = nullptr;
    int size_;
};

}  // namespace r2d2
