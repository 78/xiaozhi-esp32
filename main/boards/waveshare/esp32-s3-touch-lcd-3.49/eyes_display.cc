#include "eyes_display.h"

#include "assets/lang_config.h"
#include "display/lvgl_display/lvgl_theme.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr int32_t kEyeSpacing = 150;  // distance from screen center to each eye center
constexpr int32_t kBaseY = 8;         // leave room for the status bar on top
constexpr uint32_t kBackground = 0x000000;
constexpr uint32_t kStatusText = 0x8A94A6;

constexpr uint32_t kCyan = 0x29D8FF;
constexpr uint32_t kPink = 0xFF5FA2;
constexpr uint32_t kBlue = 0x4D8BFF;
constexpr uint32_t kRed = 0xFF3B3B;
constexpr uint32_t kBlush = 0xFF8FB0;

bool Is(const char* emotion, const char* name) { return strcmp(emotion, name) == 0; }

int32_t LerpInt(int32_t a, int32_t b, int32_t t) { return a + (b - a) * t / 1000; }

uint32_t LerpColor(uint32_t a, uint32_t b, int32_t t) {
    uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
        int32_t ca = (a >> shift) & 0xFF;
        int32_t cb = (b >> shift) & 0xFF;
        out |= static_cast<uint32_t>(LerpInt(ca, cb, t)) << shift;
    }
    return out;
}

void MakePlain(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace

EyesDisplay::~EyesDisplay() {
    DisplayLockGuard lock(this);
    if (blink_timer_ != nullptr) {
        lv_timer_delete(blink_timer_);
    }
    if (saccade_timer_ != nullptr) {
        lv_timer_delete(saccade_timer_);
    }
    lv_anim_delete(&current_, nullptr);
    lv_anim_delete(&openness_, nullptr);
    lv_anim_delete(&bounce_y_, nullptr);
    lv_anim_delete(&gaze_x_, nullptr);
}

EyesDisplay::Expression EyesDisplay::ExpressionFor(const char* emotion) {
    // Tuned for the 640x172 landscape "visor".
    //                 w    h   top  angle bottom  x    y   color  wink
    Expression neutral{120, 100, 0, 0, 0, 0, 0, kCyan, false};
    if (emotion == nullptr) {
        return neutral;
    }
    if (Is(emotion, "happy") || Is(emotion, "laughing") || Is(emotion, "funny") ||
        Is(emotion, "confident") || Is(emotion, "delicious") || Is(emotion, "silly")) {
        return {130, 92, 0, 0, 550, 0, -4, kCyan, false};
    }
    if (Is(emotion, "loving") || Is(emotion, "kissy")) {
        return {130, 92, 0, 0, 550, 0, -4, kPink, false};
    }
    if (Is(emotion, "sad") || Is(emotion, "crying")) {
        return {120, 85, 320, -200, 0, 0, 12, kBlue, false};
    }
    if (Is(emotion, "angry")) {
        return {120, 90, 360, 220, 0, 0, 4, kRed, false};
    }
    if (Is(emotion, "surprised") || Is(emotion, "shocked")) {
        return {140, 125, 0, 0, 0, 0, -2, kCyan, false};
    }
    if (Is(emotion, "thinking")) {
        return {110, 92, 200, 0, 0, -30, -14, kCyan, false};
    }
    if (Is(emotion, "confused")) {
        return {120, 92, 260, -120, 0, 26, -6, kCyan, false};
    }
    if (Is(emotion, "winking")) {
        return {125, 96, 0, 0, 300, 0, 0, kCyan, true};
    }
    if (Is(emotion, "cool")) {
        return {130, 92, 420, 0, 0, 0, 0, kCyan, false};
    }
    if (Is(emotion, "relaxed")) {
        return {125, 96, 480, 0, 200, 0, 6, kCyan, false};
    }
    if (Is(emotion, "sleepy")) {
        return {125, 96, 650, 0, 0, 0, 10, kCyan, false};
    }
    if (Is(emotion, "embarrassed")) {
        return {120, 88, 120, -80, 320, 0, 8, kBlush, false};
    }
    return neutral;
}

EyesDisplay::Expression EyesDisplay::Lerp(const Expression& a, const Expression& b, int32_t t) {
    return {
        LerpInt(a.width, b.width, t),
        LerpInt(a.height, b.height, t),
        LerpInt(a.top_cover, b.top_cover, t),
        LerpInt(a.top_angle, b.top_angle, t),
        LerpInt(a.bottom_cover, b.bottom_cover, t),
        LerpInt(a.offset_x, b.offset_x, t),
        LerpInt(a.offset_y, b.offset_y, t),
        LerpColor(a.color, b.color, t),
        t >= 500 ? b.wink_right : a.wink_right,
    };
}

void EyesDisplay::SetupUI() {
    if (face_ != nullptr) {
        return;
    }
    // The eyes are designed for a dark background, whatever theme was saved before.
    current_theme_ = LvglThemeManager::GetInstance().GetTheme("dark");
    LcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    ApplyDarkSurface();
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);

    face_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(face_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(face_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(face_, 0, 0);
    MakePlain(face_);
    lv_obj_center(face_);
    // Right above the background container, below status bar and messages.
    lv_obj_move_to_index(face_, 1);

    CreateEye(eyes_[0]);
    CreateEye(eyes_[1]);

    current_ = from_ = to_ = ExpressionFor("neutral");
    ApplyEyes();

    blink_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<EyesDisplay*>(lv_timer_get_user_data(timer));
            self->StartBlink();
            lv_timer_set_period(timer, lv_rand(2500, 6000));
        },
        3000, this);

    saccade_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<EyesDisplay*>(lv_timer_get_user_data(timer));
            self->StartSaccade();
            lv_timer_set_period(timer, lv_rand(2000, 5000));
        },
        4000, this);
}

void EyesDisplay::CreateEye(Eye& eye) {
    eye.body = lv_obj_create(face_);
    MakePlain(eye.body);
    lv_obj_set_style_bg_opa(eye.body, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(eye.body, true, 0);

    eye.top_lid = lv_obj_create(eye.body);
    MakePlain(eye.top_lid);
    lv_obj_set_style_radius(eye.top_lid, 0, 0);
    lv_obj_set_style_bg_color(eye.top_lid, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(eye.top_lid, LV_OPA_COVER, 0);

    eye.bottom_lid = lv_obj_create(eye.body);
    MakePlain(eye.bottom_lid);
    lv_obj_set_style_radius(eye.bottom_lid, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye.bottom_lid, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(eye.bottom_lid, LV_OPA_COVER, 0);
}

void EyesDisplay::ApplyDarkSurface() {
    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
    if (container_ != nullptr) {
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
        lv_obj_set_style_bg_color(container_, lv_color_hex(kBackground), 0);
    }
    if (top_bar_ != nullptr) {
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    }
    for (lv_obj_t* label :
         {network_label_, status_label_, notification_label_, mute_label_, battery_label_}) {
        if (label != nullptr) {
            lv_obj_set_style_text_color(label, lv_color_hex(kStatusText), 0);
        }
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(kBackground), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_70, 0);
    }
    if (chat_message_label_ != nullptr) {
        lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);
    }
}

void EyesDisplay::ApplyEyes() {
    if (face_ == nullptr) {
        return;
    }
    // Listening: eyes open slightly wider, attentive.
    int32_t scale = mode_ == Mode::kListening ? 108 : 100;

    for (int side = 0; side < 2; side++) {
        Eye& eye = eyes_[side];
        int32_t open = openness_;
        if (side == 1 && current_.wink_right) {
            open = std::min<int32_t>(open, 60);
        }
        int32_t w = current_.width * scale / 100;
        int32_t h = std::max<int32_t>(8, current_.height * scale / 100 * open / 1000);

        lv_obj_set_size(eye.body, w, h);
        lv_obj_set_style_radius(eye.body, std::min(w, h) * 2 / 5, 0);
        lv_obj_set_style_bg_color(eye.body, lv_color_hex(current_.color), 0);

        int32_t x = (side == 0 ? -kEyeSpacing : kEyeSpacing) + current_.offset_x + gaze_x_;
        int32_t y = kBaseY + current_.offset_y + gaze_y_ + bounce_y_;
        lv_obj_align(eye.body, LV_ALIGN_CENTER, x, y);

        int32_t top_cover = h * current_.top_cover / 1000;
        if (top_cover <= 0) {
            lv_obj_add_flag(eye.top_lid, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(eye.top_lid, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(eye.top_lid, w * 2, h);
            lv_obj_set_pos(eye.top_lid, -w / 2, top_cover - h);
            // Pivot on the lid's bottom-center so the cut line tilts around the eye's middle.
            lv_obj_set_style_transform_pivot_x(eye.top_lid, w, 0);
            lv_obj_set_style_transform_pivot_y(eye.top_lid, h, 0);
            int32_t angle = side == 0 ? current_.top_angle : -current_.top_angle;
            lv_obj_set_style_transform_rotation(eye.top_lid, angle, 0);
        }

        int32_t bottom_cover = h * current_.bottom_cover / 1000;
        if (bottom_cover <= 0) {
            lv_obj_add_flag(eye.bottom_lid, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(eye.bottom_lid, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(eye.bottom_lid, w * 3 / 2, h);
            lv_obj_set_pos(eye.bottom_lid, -w / 4, h - bottom_cover);
        }
    }
}

void EyesDisplay::TransitionTo(const Expression& target) {
    from_ = current_;
    to_ = target;
    lv_anim_delete(&current_, nullptr);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &current_);
    lv_anim_set_user_data(&a, this);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_custom_exec_cb(&a, [](lv_anim_t* anim, int32_t value) {
        auto self = static_cast<EyesDisplay*>(lv_anim_get_user_data(anim));
        self->current_ = Lerp(self->from_, self->to_, value);
        self->ApplyEyes();
    });
    lv_anim_start(&a);
}

void EyesDisplay::StartBlink() {
    if (lv_anim_get(&openness_, nullptr) != nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &openness_);
    lv_anim_set_user_data(&a, this);
    lv_anim_set_values(&a, 1000, 0);
    lv_anim_set_duration(&a, 70);
    lv_anim_set_reverse_duration(&a, 110);
    // Occasionally blink twice, it reads as more alive.
    lv_anim_set_repeat_count(&a, lv_rand(0, 4) == 0 ? 2 : 1);
    lv_anim_set_custom_exec_cb(&a, [](lv_anim_t* anim, int32_t value) {
        auto self = static_cast<EyesDisplay*>(lv_anim_get_user_data(anim));
        self->openness_ = value;
        self->ApplyEyes();
    });
    lv_anim_start(&a);
}

void EyesDisplay::StartSaccade() {
    if (mode_ != Mode::kIdle) {
        return;
    }
    gaze_from_x_ = gaze_x_;
    gaze_from_y_ = gaze_y_;
    // Mostly look back to the center, sometimes glance around.
    if (lv_rand(0, 2) == 0) {
        gaze_to_x_ = 0;
        gaze_to_y_ = 0;
    } else {
        gaze_to_x_ = static_cast<int32_t>(lv_rand(0, 80)) - 40;
        gaze_to_y_ = static_cast<int32_t>(lv_rand(0, 20)) - 10;
    }
    lv_anim_delete(&gaze_x_, nullptr);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &gaze_x_);
    lv_anim_set_user_data(&a, this);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 140);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_custom_exec_cb(&a, [](lv_anim_t* anim, int32_t value) {
        auto self = static_cast<EyesDisplay*>(lv_anim_get_user_data(anim));
        self->gaze_x_ = LerpInt(self->gaze_from_x_, self->gaze_to_x_, value);
        self->gaze_y_ = LerpInt(self->gaze_from_y_, self->gaze_to_y_, value);
        self->ApplyEyes();
    });
    lv_anim_start(&a);
}

void EyesDisplay::SetMode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    mode_ = mode;

    lv_anim_delete(&bounce_y_, nullptr);
    bounce_y_ = 0;
    if (mode_ != Mode::kIdle) {
        // Focus on the user: center the gaze.
        lv_anim_delete(&gaze_x_, nullptr);
        gaze_x_ = 0;
        gaze_y_ = 0;
    }

    if (mode_ == Mode::kSpeaking) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, &bounce_y_);
        lv_anim_set_user_data(&a, this);
        lv_anim_set_values(&a, 0, -7);
        lv_anim_set_duration(&a, 260);
        lv_anim_set_reverse_duration(&a, 260);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_custom_exec_cb(&a, [](lv_anim_t* anim, int32_t value) {
            auto self = static_cast<EyesDisplay*>(lv_anim_get_user_data(anim));
            self->bounce_y_ = value;
            self->ApplyEyes();
        });
        lv_anim_start(&a);
    }
    ApplyEyes();
}

bool EyesDisplay::SetEmotion(const char* emotion) {
    if (face_ == nullptr) {
        return false;
    }
    DisplayLockGuard lock(this);
    TransitionTo(ExpressionFor(emotion));
    return true;
}

void EyesDisplay::SetStatus(const char* status) {
    CustomLcdDisplay::SetStatus(status);
    if (face_ == nullptr || status == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        SetMode(Mode::kListening);
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        SetMode(Mode::kSpeaking);
    } else {
        SetMode(Mode::kIdle);
    }
}

void EyesDisplay::SetChatMessage(const char* role, const char* content) {
    // Only system messages (Wi-Fi setup, activation code, alerts) are shown as text.
    // The conversation itself is expressed by the eyes.
    if (role != nullptr && strcmp(role, "system") == 0) {
        CustomLcdDisplay::SetChatMessage(role, content);
    } else {
        CustomLcdDisplay::SetChatMessage(role, "");
    }
}

void EyesDisplay::SetTheme(Theme* theme) {
    CustomLcdDisplay::SetTheme(theme);
    DisplayLockGuard lock(this);
    ApplyDarkSurface();
}

void EyesDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    CustomLcdDisplay::SetPreviewImage(std::move(image));
    if (face_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    // The base class toggles the emoji box; keep it hidden and swap the eyes instead.
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_has_flag(preview_image_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(face_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
    }
}
