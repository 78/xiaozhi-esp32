#ifndef OLED_EYES_H
#define OLED_EYES_H

// Animated "desk buddy" eyes for a monochrome SSD1306 OLED (header-only).
// The eyes fill the whole screen. Two rounded rectangles with "lids" painted
// over them in the background colour. They blink, glance around and change
// shape with the emotion name the firmware already sends (happy, sad, ...).
//
// Modes:
//   Idle      standby: huge eyes covering the entire frame, no mouth.
//   Listening conversation: eyes move up a little, a small closed mouth appears.
//   Speaking  same layout, the mouth opens and closes like talking.

#include <lvgl.h>

#include <esp_random.h>
#include <math.h>
#include <string.h>

class OledEyes {
public:
    enum class Mode { Idle, Listening, Speaking };

    bool IsReady() const { return root_ != nullptr; }

    // parent: the screen. width/height: the whole panel (e.g. 128x64).
    void Create(lv_obj_t* parent, int width, int height) {
        full_w_ = width;
        full_h_ = height;

        root_ = lv_obj_create(parent);
        lv_obj_remove_style_all(root_);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_size(root_, full_w_, full_h_);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

        for (int i = 0; i < 2; i++) {
            eye_[i].o = MakeBox();
        }
        for (int i = 0; i < 2; i++) {
            lid_top_[i].o = MakeBox();
            lid_bot_[i].o = MakeBox();
        }
        mouth_.o = MakeBox();

        ApplyColors();

        SetEmotion("neutral");
        cur_ = target_;
        uint32_t now = lv_tick_get();
        next_blink_ = now + 2000;
        next_look_ = now + 1500;
        Render(true);
        timer_ = lv_timer_create(&OledEyes::TimerCb, 40, this);
    }

    void Destroy() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
        if (root_ != nullptr) {
            lv_obj_delete(root_);
            root_ = nullptr;
        }
    }

    // Hide the eyes completely (used while a system message such as the
    // activation code is on screen).
    void SetHidden(bool hidden) {
        if (root_ == nullptr || hidden == hidden_) {
            return;
        }
        hidden_ = hidden;
        if (hidden_) {
            lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
            Render(true);
        }
    }

    void SetMode(Mode mode) { mode_ = mode; }

    // Dark mode (default): white eyes on a black background.
    // Light mode: black eyes on a white background.
    void SetLight(bool light) {
        light_ = light;
        ApplyColors();
    }

    // Returns true when the emotion name is known.
    bool SetEmotion(const char* name) {
        if (name == nullptr) {
            name = "neutral";
        }
        Pose p;
        can_blink_ = true;
        can_look_ = true;
        bool known = true;
        auto is = [&](const char* s) { return strcmp(name, s) == 0; };

        if (is("neutral") || is("confident")) {
            // default pose
        } else if (is("relaxed")) {
            p.top = 22;
        } else if (is("cool")) {
            p.top = 32;
        } else if (is("happy") || is("laughing") || is("funny") || is("silly") || is("loving") ||
                   is("delicious") || is("kissy")) {
            p.bot = 48;  // upturned "happy" eyes
            p.h = 1.05f;
            p.mw = 1.35f;
            can_blink_ = false;
        } else if (is("sad") || is("crying") || is("embarrassed")) {
            p.top = 30;
            p.h = 0.8f;
            p.gap = 1.25f;
            p.ly = 3;
            p.mw = 0.7f;
            can_look_ = false;
        } else if (is("angry")) {
            p.top = 45;
            p.gap = 0.55f;
            p.mw = 0.9f;
            can_look_ = false;
        } else if (is("surprised") || is("shocked")) {
            p.w = 1.05f;
            p.h = 1.1f;
            p.gap = 1.1f;
            p.mw = 0.6f;
            can_look_ = false;
        } else if (is("thinking") || is("confused")) {
            p.lx = 1;  // look up and to the side (scaled by amplitude)
            p.ly = -1;
            p.top = 15;
            p.mw = 0.6f;
            can_look_ = false;
        } else if (is("winking")) {
            p.r = 0.12f;  // right eye closed
            can_blink_ = false;
        } else if (is("sleepy")) {
            p.top = 68;
            p.h = 0.9f;
            p.mw = 0.7f;
            can_blink_ = false;
            can_look_ = false;
        } else {
            known = false;  // unknown name: stay neutral
        }
        base_ = p;
        look_x_ = 0;
        look_y_ = 0;
        // thinking/confused store the look direction in lx/ly as a sign, convert now
        base_.lx *= kLookAmpX;
        base_.ly *= kLookAmpY;
        target_ = base_;
        return known;
    }

private:
    struct Pose {
        float w = 1, h = 1;    // eye size scale
        float top = 0;         // top lid, % of eye height
        float bot = 0;         // bottom lid, % of eye height
        float gap = 1;         // gap scale
        float lx = 0, ly = 0;  // look offset in pixels
        float r = 1;           // right eye height scale (wink)
        float mw = 1;          // mouth width scale
    };
    struct Box {
        lv_obj_t* o = nullptr;
        int x = -9999, y = -9999, w = -1, h = -1;
        bool hidden = false;
    };

    static constexpr float kLookAmpX = 4.0f;
    static constexpr float kLookAmpY = 3.0f;

    lv_obj_t* root_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    Box eye_[2], lid_top_[2], lid_bot_[2], mouth_;
    Pose base_, target_, cur_;
    Mode mode_ = Mode::Idle;
    int full_w_ = 128, full_h_ = 64;
    bool light_ = false;
    bool hidden_ = false;
    bool can_blink_ = true, can_look_ = true;
    int look_x_ = 0, look_y_ = 0;  // -1, 0, 1 glance direction
    float layout_ = 0;             // 0 = standby (full frame), 1 = conversation
    float mouth_show_ = 0;         // 0 = no mouth, 1 = mouth visible
    float mouth_open_ = 0;         // 0 = closed, 1 = wide open
    float mouth_target_ = 0;
    uint32_t next_blink_ = 0, next_look_ = 0, blink_until_ = 0, next_mouth_ = 0;

    lv_obj_t* MakeBox() {
        lv_obj_t* o = lv_obj_create(root_);
        lv_obj_remove_style_all(o);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(o, 0, 0);
        return o;
    }

    // The lids must match the background and the eyes/mouth must match the foreground.
    void ApplyColors() {
        if (root_ == nullptr) {
            return;
        }
        lv_color_t bg = light_ ? lv_color_white() : lv_color_black();
        lv_color_t fg = light_ ? lv_color_black() : lv_color_white();
        lv_obj_set_style_bg_color(root_, bg, 0);
        for (int i = 0; i < 2; i++) {
            lv_obj_set_style_bg_color(eye_[i].o, fg, 0);
            lv_obj_set_style_bg_color(lid_top_[i].o, bg, 0);
            lv_obj_set_style_bg_color(lid_bot_[i].o, bg, 0);
        }
        lv_obj_set_style_bg_color(mouth_.o, fg, 0);
        lv_obj_invalidate(root_);
    }

    static uint32_t Rand(uint32_t n) { return n == 0 ? 0 : esp_random() % n; }

    static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    static void TimerCb(lv_timer_t* t) { static_cast<OledEyes*>(lv_timer_get_user_data(t))->Tick(); }

    static float Ease(float cur, float tgt, float k = 0.45f) {
        float d = tgt - cur;
        if (fabsf(d) < 0.02f) {
            return tgt;
        }
        return cur + d * k;
    }

    void Tick() {
        if (hidden_) {
            return;
        }
        uint32_t now = lv_tick_get();

        if (can_blink_ && now >= next_blink_ && now >= blink_until_) {
            blink_until_ = now + 140;
            next_blink_ = now + 2500 + Rand(3500);
        }
        if (can_look_ && now >= next_look_) {
            look_x_ = (int)Rand(3) - 1;
            look_y_ = (int)Rand(3) - 1;
            if (look_y_ != 0 && Rand(2) == 0) {
                look_y_ = 0;  // mostly glance sideways
            }
            next_look_ = now + 1200 + Rand(2800);
        }

        target_ = base_;
        if (can_look_) {
            target_.lx = look_x_ * kLookAmpX;
            target_.ly = look_y_ * kLookAmpY;
        }
        if (now < blink_until_) {
            target_.h = base_.h * 0.1f;
            target_.top = 0;
            target_.bot = 0;
        }

        cur_.w = Ease(cur_.w, target_.w);
        cur_.h = Ease(cur_.h, target_.h);
        cur_.top = Ease(cur_.top, target_.top);
        cur_.bot = Ease(cur_.bot, target_.bot);
        cur_.gap = Ease(cur_.gap, target_.gap);
        cur_.lx = Ease(cur_.lx, target_.lx);
        cur_.ly = Ease(cur_.ly, target_.ly);
        cur_.r = Ease(cur_.r, target_.r);
        cur_.mw = Ease(cur_.mw, target_.mw);

        // Standby = full frame, conversation = eyes up + mouth below.
        bool talking_layout = mode_ != Mode::Idle;
        layout_ = Ease(layout_, talking_layout ? 1.0f : 0.0f, 0.35f);
        mouth_show_ = Ease(mouth_show_, talking_layout ? 1.0f : 0.0f, 0.35f);

        // While speaking, pick a new random mouth opening every ~100 ms.
        if (mode_ == Mode::Speaking) {
            if (now >= next_mouth_) {
                static const float kLevels[] = {0.1f, 0.35f, 0.6f, 0.85f, 1.0f};
                mouth_target_ = kLevels[Rand(5)];
                next_mouth_ = now + 80 + Rand(90);
            }
        } else {
            mouth_target_ = 0;
        }
        mouth_open_ = Ease(mouth_open_, mouth_target_, 0.6f);

        Render(false);
    }

    static void Place(Box& b, int x, int y, int w, int h, int radius, bool force) {
        if (h < 1 || w < 1) {
            if (!b.hidden || force) {
                lv_obj_add_flag(b.o, LV_OBJ_FLAG_HIDDEN);
                b.hidden = true;
            }
            return;
        }
        if (b.hidden || force) {
            lv_obj_remove_flag(b.o, LV_OBJ_FLAG_HIDDEN);
            b.hidden = false;
        }
        if (force || x != b.x || y != b.y) {
            lv_obj_set_pos(b.o, x, y);
            b.x = x;
            b.y = y;
        }
        if (force || w != b.w || h != b.h) {
            lv_obj_set_size(b.o, w, h);
            lv_obj_set_style_radius(b.o, radius < h / 2 ? radius : h / 2, 0);
            b.w = w;
            b.h = h;
        }
    }

    void Render(bool force) {
        if (root_ == nullptr) {
            return;
        }
        // Standby: two huge eyes that cover the whole 128x64 frame.
        // Conversation: eyes shrink a little and move up to make room for the mouth.
        const int ew = (int)lroundf(Lerp(54, 52, layout_));
        const int eh = (int)lroundf(Lerp(54, 38, layout_));
        const int gap0 = (int)lroundf(Lerp(14, 16, layout_));
        const int radius = (int)lroundf(Lerp(16, 14, layout_));
        const int cy = (int)lroundf(Lerp(full_h_ / 2.0f, 22, layout_)) + (int)lroundf(cur_.ly);

        int w = (int)lroundf(ew * cur_.w);
        int gap = (int)lroundf(gap0 * cur_.gap);
        int max_total = full_w_ - 2;
        if (2 * w + gap > max_total) {
            w = (max_total - gap) / 2;  // never run off the screen
        }
        int total = 2 * w + gap;
        int x0 = (full_w_ - total) / 2 + (int)lroundf(cur_.lx);
        if (x0 < 1) {
            x0 = 1;
        }
        if (x0 + total > full_w_ - 1) {
            x0 = full_w_ - 1 - total;
        }

        for (int i = 0; i < 2; i++) {
            float hs = cur_.h * (i == 1 ? cur_.r : 1.0f);
            int h = (int)lroundf(eh * hs);
            if (h < 2) {
                h = 2;  // a closed eye is a thin line
            }
            if (h > full_h_ - 2) {
                h = full_h_ - 2;
            }
            int x = x0 + i * (w + gap);
            int y = cy - h / 2;
            if (y < 0) {
                y = 0;
            }
            if (y + h > full_h_) {
                y = full_h_ - h;
            }
            Place(eye_[i], x, y, w, h, radius, force);

            int ht = (int)lroundf(h * cur_.top / 100.0f);
            if (ht < 1) {
                Place(lid_top_[i], x - 2, y - 1, w + 4, 0, 0, force);  // hide
            } else {
                Place(lid_top_[i], x - 2, y - 1, w + 4, ht + 1, 0, force);
            }
            int hb = (int)lroundf(h * cur_.bot / 100.0f);
            if (hb < 1) {
                Place(lid_bot_[i], x - 2, y + h, w + 4, 0, 0, force);  // hide
            } else {
                Place(lid_bot_[i], x - 2, y + h - hb, w + 4, hb + 2, 0, force);
            }
        }

        // Small mouth centred under the eyes: a thin line when quiet,
        // opens up to ~13 px while speaking.
        int mw = (int)lroundf(22 * cur_.mw * mouth_show_);
        int mh = (int)lroundf((3 + 10 * mouth_open_) * mouth_show_);
        if (mh < 2 || mw < 4) {
            Place(mouth_, 0, 0, 0, 0, 0, force);  // hide
        } else {
            Place(mouth_, (full_w_ - mw) / 2, 53 - mh / 2, mw, mh, mh / 2, force);
        }
    }
};

#endif  // OLED_EYES_H
