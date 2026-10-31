#ifndef OLED_EYES_H
#define OLED_EYES_H

// Animated "desk buddy" eyes for a monochrome SSD1306 OLED (header-only).
// Two white rounded rectangles on black, with black "lids" over them.
// The eyes blink, glance around and change shape with the emotion name that
// the firmware already sends (happy, sad, angry, thinking, ...).

#include <lvgl.h>

#include <esp_random.h>
#include <math.h>
#include <string.h>

class OledEyes {
public:
    bool IsReady() const { return root_ != nullptr; }

    // parent: the LVGL content area. width/height: size of the area the eyes may use.
    // subtitle_box: the object that holds the chat text; while it is visible the eyes
    // shrink into the left column automatically.
    void Create(lv_obj_t* parent, int width, int height, lv_obj_t* subtitle_box) {
        subtitle_box_ = subtitle_box;
        full_w_ = width;
        full_h_ = height;

        root_ = lv_obj_create(parent);
        lv_obj_remove_style_all(root_);
        lv_obj_add_flag(root_, LV_OBJ_FLAG_FLOATING);  // ignored by the parent's flex layout
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_size(root_, full_w_, full_h_);
        lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

        for (int i = 0; i < 2; i++) {
            eye_[i].o = MakeBox(lv_color_white());
        }
        for (int i = 0; i < 2; i++) {
            lid_top_[i].o = MakeBox(lv_color_black());
            lid_bot_[i].o = MakeBox(lv_color_black());
        }

        SetEmotion("neutral");
        cur_ = target_;
        uint32_t now = lv_tick_get();
        next_blink_ = now + 2000;
        next_look_ = now + 1500;
        Render(true);
        timer_ = lv_timer_create(&OledEyes::TimerCb, 40, this);
    }

    // Eyes use the whole area normally; compact mode squeezes them into the
    // left 32 px column so a chat subtitle can be shown on the right.
    void SetCompact(bool compact) {
        if (root_ == nullptr || compact == compact_) {
            return;
        }
        compact_ = compact;
        lv_obj_set_size(root_, compact_ ? 32 : full_w_, full_h_);
        Render(true);
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
            can_blink_ = false;
        } else if (is("sad") || is("crying") || is("embarrassed")) {
            p.top = 30;
            p.h = 0.8f;
            p.gap = 1.25f;
            p.ly = 4;
            can_look_ = false;
        } else if (is("angry")) {
            p.top = 45;
            p.gap = 0.55f;
            can_look_ = false;
        } else if (is("surprised") || is("shocked")) {
            p.w = 1.12f;
            p.h = 1.3f;
            p.gap = 1.1f;
            can_look_ = false;
        } else if (is("thinking") || is("confused")) {
            p.lx = 1;   // look up and to the side (scaled by amplitude)
            p.ly = -1;
            p.top = 15;
            can_look_ = false;
        } else if (is("winking")) {
            p.r = 0.12f;  // right eye closed
            can_blink_ = false;
        } else if (is("sleepy")) {
            p.top = 68;
            p.h = 0.9f;
            can_blink_ = false;
            can_look_ = false;
        } else {
            known = false;  // unknown name: stay neutral
        }
        base_ = p;
        look_x_ = 0;
        look_y_ = 0;
        target_ = base_;
        // thinking/confused store look direction in lx/ly as a sign, convert now
        ApplyLookAmplitudeToBase();
        return known;
    }

private:
    struct Pose {
        float w = 1, h = 1;   // eye size scale
        float top = 0;        // top lid, % of eye height
        float bot = 0;        // bottom lid, % of eye height
        float gap = 1;        // gap scale
        float lx = 0, ly = 0; // look offset (pixels after ApplyLookAmplitudeToBase)
        float r = 1;          // right eye height scale (wink)
    };
    struct Box {
        lv_obj_t* o = nullptr;
        int x = -9999, y = -9999, w = -1, h = -1;
        bool hidden = false;
    };

    lv_obj_t* root_ = nullptr;
    lv_obj_t* subtitle_box_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    Box eye_[2], lid_top_[2], lid_bot_[2];
    Pose base_, target_, cur_;
    int full_w_ = 128, full_h_ = 48;
    bool compact_ = false;
    bool can_blink_ = true, can_look_ = true;
    int look_x_ = 0, look_y_ = 0;  // -1, 0, 1 glance direction
    uint32_t next_blink_ = 0, next_look_ = 0, blink_until_ = 0;

    lv_obj_t* MakeBox(lv_color_t color) {
        lv_obj_t* o = lv_obj_create(root_);
        lv_obj_remove_style_all(o);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(o, color, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(o, 0, 0);
        return o;
    }

    static uint32_t Rand(uint32_t n) { return n == 0 ? 0 : esp_random() % n; }

    float LookAmpX() const { return compact_ ? 2.0f : 8.0f; }
    float LookAmpY() const { return compact_ ? 2.0f : 4.0f; }

    void ApplyLookAmplitudeToBase() {
        base_.lx *= LookAmpX();
        base_.ly *= LookAmpY();
        target_ = base_;
    }

    static void TimerCb(lv_timer_t* t) {
        static_cast<OledEyes*>(lv_timer_get_user_data(t))->Tick();
    }

    static float Ease(float cur, float tgt) {
        float d = tgt - cur;
        if (fabsf(d) < 0.05f) {
            return tgt;
        }
        return cur + d * 0.45f;
    }

    void Tick() {
        uint32_t now = lv_tick_get();

        if (subtitle_box_ != nullptr) {
            SetCompact(!lv_obj_has_flag(subtitle_box_, LV_OBJ_FLAG_HIDDEN));
        }

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
            target_.lx = look_x_ * LookAmpX();
            target_.ly = look_y_ * LookAmpY();
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
        const int area_w = compact_ ? 32 : full_w_;
        const int ew = compact_ ? 13 : 36;
        const int eh = compact_ ? 26 : 32;
        const int gap0 = compact_ ? 4 : 22;
        const int radius = compact_ ? 5 : 11;

        int w = (int)lroundf(ew * cur_.w);
        int gap = (int)lroundf(gap0 * cur_.gap);
        int total = 2 * w + gap;
        int x0 = (area_w - total) / 2 + (int)lroundf(cur_.lx);
        int cy = full_h_ / 2 + (int)lroundf(cur_.ly);

        for (int i = 0; i < 2; i++) {
            float hs = cur_.h * (i == 1 ? cur_.r : 1.0f);
            int h = (int)lroundf(eh * hs);
            if (h < 2) {
                h = 2;  // a closed eye is a thin line
            }
            int x = x0 + i * (w + gap);
            int y = cy - h / 2;
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
    }
};

#endif  // OLED_EYES_H
