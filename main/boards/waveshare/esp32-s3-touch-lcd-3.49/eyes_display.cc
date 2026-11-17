#include "eyes_display.h"

#include "assets/lang_config.h"
#include "display/lvgl_display/lvgl_theme.h"

#include <material_symbols.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {

// Layout (640x172 landscape visor)
constexpr int32_t kEyeSpacing = 150;  // distance from screen center to each eye center
constexpr int32_t kBaseY = 8;         // leave room for the status bar on top

// Colors
constexpr uint32_t kBackground = 0x000000;
constexpr uint32_t kStatusText = 0x8A94A6;
constexpr uint32_t kPink = 0xFF5FA2;
constexpr uint32_t kBlue = 0x4D8BFF;
constexpr uint32_t kAngry = 0xFF5A2E;
constexpr uint32_t kOrange = 0xFFA040;
constexpr uint32_t kBlush = 0xFF8FB0;
constexpr uint32_t kGround = 0x1B6F80;
constexpr uint32_t kObstacleColor = 0xFFB347;

// Behaviour timing
constexpr uint32_t kTickMs = 50;
constexpr uint32_t kBoredAfterMs = 60 * 1000;
constexpr uint32_t kFirstActivityAfterMs = 2 * 60 * 1000;
constexpr uint32_t kActivityGapMinMs = 90 * 1000;
constexpr uint32_t kActivityGapMaxMs = 150 * 1000;
constexpr uint32_t kSleepAfterMs = 10 * 60 * 1000;
constexpr uint32_t kSleepAfterNightMs = 5 * 60 * 1000;

// Touch
constexpr uint32_t kAnnoyanceDecayMs = 3000;  // one tap is forgiven every 3 s
constexpr int32_t kAnnoyedTaps = 3;
constexpr int32_t kAngryTaps = 6;

// Runner game
constexpr uint32_t kGameDurationMs = 45 * 1000;
constexpr int32_t kGroundY = 150;
constexpr int32_t kRunnerX = 70;
constexpr int32_t kRunnerW = 34;
constexpr int32_t kRunnerH = 30;
constexpr int32_t kJump10 = 130;   // initial jump speed, x10 px per tick
constexpr int32_t kGravity10 = 13;
constexpr int32_t kStartSpeed = 8;  // px per tick
constexpr int32_t kMaxSpeed = 14;
constexpr uint32_t kMissChancePct = 5;  // it is not perfect: sometimes it trips

bool Is(const char* a, const char* b) { return strcmp(a, b) == 0; }

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

// Sine wave of the given period (ms) and amplitude.
int32_t Wave(uint32_t time_ms, uint32_t period_ms, int32_t amplitude) {
    auto angle = static_cast<int16_t>((time_ms % period_ms) * 360 / period_ms);
    return lv_trigo_sin(angle) * amplitude / 32767;
}

int32_t Jitter(int32_t amplitude) {
    return static_cast<int32_t>(lv_rand(0, amplitude * 2)) - amplitude;
}

bool IsNight() {
    time_t now = time(nullptr);
    struct tm local = {};
    localtime_r(&now, &local);
    if (local.tm_year < 124) {
        return false;  // clock not synced yet
    }
    return local.tm_hour >= 23 || local.tm_hour < 7;
}

void MakePlain(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t* CreateBlob(lv_obj_t* parent, uint32_t color, int32_t radius) {
    lv_obj_t* obj = lv_obj_create(parent);
    MakePlain(obj);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

}  // namespace

EyesDisplay::~EyesDisplay() {
    DisplayLockGuard lock(this);
    for (lv_timer_t* timer : {blink_timer_, saccade_timer_, tick_timer_}) {
        if (timer != nullptr) {
            lv_timer_delete(timer);
        }
    }
    lv_anim_delete(&current_, nullptr);
    lv_anim_delete(&openness_, nullptr);
    lv_anim_delete(&gaze_x_, nullptr);
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

EyesDisplay::Expression EyesDisplay::ExpressionFor(const char* name) {
    if (name == nullptr) {
        return {};
    }
    // Server emotions (sent by the LLM with each reply)
    if (Is(name, "happy")) {
        return {.width = 130, .height = 92, .bottom_cover = 550, .offset_y = -4};
    }
    if (Is(name, "laughing")) {
        return {.width = 135, .height = 92, .bottom_cover = 620, .offset_y = -6,
                .motion = Motion::kBounce};
    }
    if (Is(name, "funny")) {
        return {.width = 130, .height = 92, .bottom_cover = 550, .offset_y = -4,
                .right_height = 650, .motion = Motion::kBounce};
    }
    if (Is(name, "silly")) {
        return {.width = 115, .height = 100, .right_height = 780, .inward = 30,
                .motion = Motion::kSway};
    }
    if (Is(name, "confident")) {
        return {.width = 125, .height = 90, .top_cover = 320, .top_angle = -80,
                .bottom_cover = 250};
    }
    if (Is(name, "delicious")) {
        return {.width = 130, .height = 92, .bottom_cover = 550, .offset_y = 10, .blush = true};
    }
    if (Is(name, "loving")) {
        return {.width = 125, .height = 105, .bottom_cover = 300, .color = kPink,
                .motion = Motion::kPulse, .blush = true};
    }
    if (Is(name, "kissy")) {
        return {.width = 125, .height = 96, .bottom_cover = 300, .color = kPink,
                .wink_right = true, .blush = true};
    }
    if (Is(name, "sad")) {
        return {.width = 120, .height = 85, .top_cover = 320, .top_angle = -200, .offset_y = 12,
                .color = kBlue};
    }
    if (Is(name, "crying")) {
        return {.width = 120, .height = 85, .top_cover = 320, .top_angle = -200, .offset_y = 12,
                .color = kBlue, .motion = Motion::kTremble, .tears = true};
    }
    if (Is(name, "angry")) {
        // Glare: narrow slits squeezed from above and below, sharp V brow, held-back tremble.
        return {.width = 130, .height = 92, .top_cover = 420, .top_angle = 300,
                .bottom_cover = 260, .offset_y = 2, .color = kAngry, .motion = Motion::kTremble};
    }
    if (Is(name, "surprised")) {
        return {.width = 140, .height = 125, .offset_y = -2};
    }
    if (Is(name, "shocked") || Is(name, "startled")) {
        return {.width = 145, .height = 128, .offset_y = -4, .motion = Motion::kTremble};
    }
    if (Is(name, "thinking")) {
        // Intrigue: narrowed eyes looking up to one side, scanning.
        return {.width = 110, .height = 92, .top_cover = 220, .offset_x = -30, .offset_y = -14,
                .motion = Motion::kScan};
    }
    if (Is(name, "confused")) {
        // Doubt: uneven eyes, tilted lids and a question mark.
        return {.width = 120, .height = 100, .top_cover = 200, .top_angle = -120, .offset_x = 20,
                .right_height = 720, .motion = Motion::kSway, .question = true};
    }
    if (Is(name, "winking")) {
        return {.width = 125, .height = 96, .bottom_cover = 300, .wink_right = true};
    }
    if (Is(name, "cool")) {
        return {.width = 130, .height = 92, .top_cover = 420, .offset_x = 22};
    }
    if (Is(name, "relaxed")) {
        return {.width = 125, .height = 96, .top_cover = 480, .bottom_cover = 200, .offset_y = 6,
                .motion = Motion::kBreath};
    }
    if (Is(name, "sleepy")) {
        return {.width = 125, .height = 96, .top_cover = 650, .offset_y = 10,
                .motion = Motion::kBreath};
    }
    if (Is(name, "embarrassed")) {
        return {.width = 115, .height = 88, .top_cover = 120, .top_angle = -80,
                .bottom_cover = 320, .offset_x = -26, .offset_y = 10, .color = kBlush,
                .blush = true};
    }

    // Local states (decided on the device)
    if (Is(name, "annoyed")) {
        // Fed up: flat lids, side-eye.
        return {.width = 125, .height = 80, .top_cover = 430, .top_angle = 90, .offset_x = 32,
                .color = kOrange};
    }
    if (Is(name, "bored")) {
        return {.width = 125, .height = 96, .top_cover = 520, .offset_x = -34, .offset_y = 6};
    }
    if (Is(name, "asleep")) {
        return {.width = 125, .height = 96, .top_cover = 930, .offset_y = 14,
                .motion = Motion::kBreath, .zzz = true};
    }
    if (Is(name, "reading")) {
        return {.width = 120, .height = 90, .top_cover = 250};
    }
    if (Is(name, "presneeze")) {
        return {.width = 125, .height = 96, .top_cover = 600, .top_angle = -60, .offset_y = -6,
                .motion = Motion::kTremble};
    }
    if (Is(name, "curious")) {
        return {.width = 135, .height = 115};
    }
    if (Is(name, "dizzy")) {
        return {.width = 120, .height = 100, .right_height = 650, .motion = Motion::kSway};
    }
    return {};  // neutral and anything unknown
}

EyesDisplay::Expression EyesDisplay::Lerp(const Expression& a, const Expression& b, int32_t t) {
    Expression out = b;  // flags, motion and decor switch immediately
    out.width = LerpInt(a.width, b.width, t);
    out.height = LerpInt(a.height, b.height, t);
    out.top_cover = LerpInt(a.top_cover, b.top_cover, t);
    out.top_angle = LerpInt(a.top_angle, b.top_angle, t);
    out.bottom_cover = LerpInt(a.bottom_cover, b.bottom_cover, t);
    out.offset_x = LerpInt(a.offset_x, b.offset_x, t);
    out.offset_y = LerpInt(a.offset_y, b.offset_y, t);
    out.color = LerpColor(a.color, b.color, t);
    out.wink_right = t >= 500 ? b.wink_right : a.wink_right;
    out.right_height = LerpInt(a.right_height, b.right_height, t);
    out.inward = LerpInt(a.inward, b.inward, t);
    return out;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void EyesDisplay::SetupUI() {
    if (face_ != nullptr) {
        return;
    }
    // The eyes are designed for a dark background, whatever theme was saved before.
    current_theme_ = LvglThemeManager::GetInstance().GetTheme("dark");
    CustomLcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    ApplyDarkSurface();
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    // The bars are plain lv_obj (clickable by default) and sit above the face: let touches
    // through so the whole screen reaches the eyes.
    for (lv_obj_t* bar : {top_bar_, status_bar_, bottom_bar_}) {
        if (bar != nullptr) {
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
            for (uint32_t i = 0; i < lv_obj_get_child_count(bar); i++) {
                lv_obj_remove_flag(lv_obj_get_child(bar, i), LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }

    auto touch_cb = [](lv_event_t* e) {
        static_cast<EyesDisplay*>(lv_event_get_user_data(e))->OnTouchEvent(e);
    };

    face_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(face_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(face_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(face_, 0, 0);
    MakePlain(face_);
    lv_obj_add_flag(face_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(face_, touch_cb, LV_EVENT_ALL, this);
    lv_obj_center(face_);
    // Right above the background container, below status bar and messages.
    lv_obj_move_to_index(face_, 1);

    CreateEye(eyes_[0]);
    CreateEye(eyes_[1]);
    CreateDecor();
    CreateGame();
    lv_obj_add_event_cb(game_, touch_cb, LV_EVENT_ALL, this);

    uint32_t now = lv_tick_get();
    last_interaction_ = now;
    next_activity_ = now + kFirstActivityAfterMs;
    target_name_ = "neutral";
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

    tick_timer_ = lv_timer_create(
        [](lv_timer_t* timer) { static_cast<EyesDisplay*>(lv_timer_get_user_data(timer))->Tick(); },
        kTickMs, this);
}

void EyesDisplay::CreateEye(Eye& eye) {
    eye.body = CreateBlob(face_, to_.color, 30);
    lv_obj_set_style_clip_corner(eye.body, true, 0);
    eye.top_lid = CreateBlob(eye.body, kBackground, 0);
    eye.bottom_lid = CreateBlob(eye.body, kBackground, LV_RADIUS_CIRCLE);
}

void EyesDisplay::CreateDecor() {
    auto theme = static_cast<LvglTheme*>(current_theme_);

    for (int i = 0; i < 2; i++) {
        blush_[i] = CreateBlob(face_, kPink, LV_RADIUS_CIRCLE);
        lv_obj_set_size(blush_[i], 44, 14);
        lv_obj_set_style_bg_opa(blush_[i], LV_OPA_50, 0);
        lv_obj_add_flag(blush_[i], LV_OBJ_FLAG_HIDDEN);

        tears_[i] = CreateBlob(face_, 0x7FB8FF, 5);
        lv_obj_set_size(tears_[i], 9, 15);
        lv_obj_add_flag(tears_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Text labels (Zzz, score) inherit the screen font, which SetTheme rebinds when assets
    // replace the theme font. Only the icon label holds an explicit font, rebound in SetTheme.
    question_ = lv_label_create(face_);
    lv_obj_set_style_text_font(question_, theme->large_icon_font()->font(), 0);
    lv_obj_set_style_text_color(question_, lv_color_white(), 0);
    const char* help = material_symbols_get_utf8("help");
    lv_label_set_text(question_, help != nullptr ? help : "?");
    lv_obj_add_flag(question_, LV_OBJ_FLAG_HIDDEN);

    const char* letters[3] = {"z", "Z", "Z"};
    for (int i = 0; i < 3; i++) {
        zzz_[i] = lv_label_create(face_);
        lv_obj_set_style_text_color(zzz_[i], lv_color_hex(kStatusText), 0);
        lv_label_set_text(zzz_[i], letters[i]);
        lv_obj_add_flag(zzz_[i], LV_OBJ_FLAG_HIDDEN);
    }

    dot_ = CreateBlob(face_, 0xFFFFFF, LV_RADIUS_CIRCLE);
    lv_obj_set_size(dot_, 14, 14);
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
}

void EyesDisplay::CreateGame() {
    game_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(game_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(game_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(game_, 0, 0);
    MakePlain(game_);
    lv_obj_add_flag(game_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(game_);
    lv_obj_move_to_index(game_, 2);  // above the face, below the status bar
    lv_obj_add_flag(game_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* ground = CreateBlob(game_, kGround, 0);
    lv_obj_set_size(ground, LV_HOR_RES, 2);
    lv_obj_set_pos(ground, 0, kGroundY);

    // The runner is a little blob wearing the same eyes as the visor.
    runner_ = CreateBlob(game_, to_.color, 10);
    lv_obj_set_size(runner_, kRunnerW, kRunnerH);
    lv_obj_set_pos(runner_, kRunnerX, kGroundY - kRunnerH);
    for (int32_t eye_x : {9, 20}) {
        lv_obj_t* eye = CreateBlob(runner_, kBackground, 3);
        lv_obj_set_size(eye, 6, 12);
        lv_obj_set_pos(eye, eye_x, 7);
    }

    for (auto& obstacle : obstacles_) {
        obstacle.obj = CreateBlob(game_, kObstacleColor, 3);
        lv_obj_add_flag(obstacle.obj, LV_OBJ_FLAG_HIDDEN);
    }

    score_label_ = lv_label_create(game_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(kStatusText), 0);
    lv_label_set_text(score_label_, "0");
    lv_obj_align(score_label_, LV_ALIGN_TOP_RIGHT, -16, 28);
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

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void EyesDisplay::ApplyEyes() {
    if (face_ == nullptr) {
        return;
    }
    uint32_t now = lv_tick_get();
    // Listening: eyes open slightly wider, attentive.
    int32_t scale = (mode_ == Mode::kListening ? 1080 : 1000) * motion_scale_ / 1000;
    int32_t base_x = current_.offset_x + gaze_x_ + motion_x_ + activity_x_ + touch_x_;
    int32_t base_y = kBaseY + current_.offset_y + gaze_y_ + motion_y_ + activity_y_ + touch_y_;

    int32_t center_x[2];
    int32_t full_h[2];
    int32_t width = current_.width * scale / 1000;

    for (int side = 0; side < 2; side++) {
        Eye& eye = eyes_[side];
        int32_t open = openness_;
        if (side == 1 && current_.wink_right) {
            open = std::min<int32_t>(open, 60);
        }
        int32_t h_full = current_.height * scale / 1000;
        if (side == 1) {
            h_full = h_full * current_.right_height / 1000;
        }
        int32_t w = width;
        int32_t h = std::max<int32_t>(8, h_full * open / 1000);
        full_h[side] = h_full;

        lv_obj_set_size(eye.body, w, h);
        lv_obj_set_style_radius(eye.body, std::min(w, h) * 2 / 5, 0);
        lv_obj_set_style_bg_color(eye.body, lv_color_hex(current_.color), 0);

        int32_t spacing = kEyeSpacing - current_.inward;
        center_x[side] = (side == 0 ? -spacing : spacing) + base_x;
        lv_obj_align(eye.body, LV_ALIGN_CENTER, center_x[side], base_y);

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

    // Decorations follow the eyes.
    for (int side = 0; side < 2; side++) {
        if (current_.blush) {
            lv_obj_remove_flag(blush_[side], LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(blush_[side], LV_ALIGN_CENTER, center_x[side],
                         base_y + full_h[side] / 2 + 12);
        } else {
            lv_obj_add_flag(blush_[side], LV_OBJ_FLAG_HIDDEN);
        }

        if (current_.tears) {
            // Each tear falls and fades, then starts again; the two are out of phase.
            uint32_t phase = (now + side * 450) % 900;
            int32_t outer = side == 0 ? -width / 3 : width / 3;
            lv_obj_remove_flag(tears_[side], LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(tears_[side], LV_ALIGN_CENTER, center_x[side] + outer,
                         base_y + full_h[side] / 2 + static_cast<int32_t>(phase * 40 / 900));
            lv_obj_set_style_opa(tears_[side], 255 - phase * 255 / 900, 0);
        } else {
            lv_obj_add_flag(tears_[side], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (current_.question) {
        lv_obj_remove_flag(question_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(question_, LV_ALIGN_CENTER, center_x[1] + width / 2 + 18,
                     base_y - full_h[1] / 2 - 6 + Wave(now, 1400, 4));
    } else {
        lv_obj_add_flag(question_, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < 3; i++) {
        if (current_.zzz) {
            // Letters drift up and fade, one after another.
            uint32_t phase = (now + i * 700) % 2100;
            lv_obj_remove_flag(zzz_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(zzz_[i], LV_ALIGN_CENTER, center_x[1] + width / 2 + 8 + i * 12,
                         base_y - 16 - static_cast<int32_t>(phase * 40 / 2100));
            lv_obj_set_style_opa(zzz_[i], 255 - phase * 255 / 2100, 0);
        } else {
            lv_obj_add_flag(zzz_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void EyesDisplay::TransitionTo(const char* name) {
    target_name_ = name;
    from_ = current_;
    to_ = ExpressionFor(name);
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

const char* EyesDisplay::ResolveExpressionName(uint32_t now) {
    if (asleep_) {
        return "asleep";
    }
    if (petting_) {
        return "loving";
    }
    if (reaction_ != nullptr) {
        if (static_cast<int32_t>(reaction_until_ - now) > 0) {
            return reaction_;
        }
        reaction_ = nullptr;
        if (queued_reaction_ != nullptr) {
            reaction_ = queued_reaction_;
            reaction_until_ = now + queued_duration_;
            queued_reaction_ = nullptr;
            return reaction_;
        }
    }
    switch (activity_) {
        case Activity::kJump:
            return "happy";
        case Activity::kChase:
            return "curious";
        case Activity::kRead:
            return "reading";
        case Activity::kSneeze: {
            uint32_t elapsed = now - activity_start_;
            return elapsed < 1000 ? "presneeze" : elapsed < 1400 ? "shocked" : "relaxed";
        }
        case Activity::kGame:
        case Activity::kNone:
            break;
    }
    if (bored_ && mode_ == Mode::kIdle) {
        return "bored";
    }
    return server_emotion_.c_str();
}

void EyesDisplay::RefreshExpression(uint32_t now) {
    const char* name = ResolveExpressionName(now);
    if (target_name_ != name) {
        TransitionTo(name);
    }
}

// ---------------------------------------------------------------------------
// Ambient animations
// ---------------------------------------------------------------------------

void EyesDisplay::StartBlink() {
    if (asleep_ || activity_ == Activity::kGame || lv_anim_get(&openness_, nullptr) != nullptr) {
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
    if (mode_ != Mode::kIdle || activity_ != Activity::kNone || asleep_ || pressing_ ||
        to_.motion == Motion::kScan) {
        return;
    }
    gaze_from_x_ = gaze_x_;
    gaze_from_y_ = gaze_y_;
    // Mostly look back to the center, sometimes glance around.
    if (lv_rand(0, 2) == 0) {
        gaze_to_x_ = 0;
        gaze_to_y_ = 0;
    } else {
        gaze_to_x_ = Jitter(40);
        gaze_to_y_ = Jitter(10);
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

void EyesDisplay::Tick() {
    if (face_ == nullptr || lv_obj_has_flag(preview_image_, LV_OBJ_FLAG_HIDDEN) == false) {
        return;
    }
    uint32_t now = lv_tick_get();
    UpdateIdle(now);
    if (activity_ == Activity::kGame) {
        UpdateGame(now);
        return;
    }
    UpdateActivity(now);
    RefreshExpression(now);
    UpdateMotion(now);
}

void EyesDisplay::UpdateMotion(uint32_t now) {
    int32_t x = 0;
    int32_t y = 0;
    int32_t scale = 1000;
    switch (to_.motion) {
        case Motion::kShake:
            x = Jitter(4);
            break;
        case Motion::kTremble:
            x = Jitter(2);
            y = Jitter(1);
            break;
        case Motion::kBounce:
            y = -std::abs(Wave(now, 400, 8));
            break;
        case Motion::kPulse:
            scale = 1000 + Wave(now, 900, 60);
            break;
        case Motion::kScan:
            x = Wave(now, 3000, 22);
            break;
        case Motion::kSway:
            x = Wave(now, 2000, 10);
            break;
        case Motion::kBreath:
            y = Wave(now, 3200, 3);
            break;
        case Motion::kNone:
            break;
    }
    if (mode_ == Mode::kSpeaking) {
        y -= std::abs(Wave(now, 520, 7));
    }

    bool animating_decor = to_.tears || to_.zzz || to_.question;
    bool changed = x != motion_x_ || y != motion_y_ || scale != motion_scale_;
    motion_x_ = x;
    motion_y_ = y;
    motion_scale_ = scale;
    // Only redraw when something moved: the panel is redrawn in full on every change.
    if (changed || animating_decor || activity_ != Activity::kNone) {
        ApplyEyes();
    }
}

void EyesDisplay::UpdateIdle(uint32_t now) {
    if (mode_ != Mode::kIdle) {
        return;
    }
    uint32_t idle = now - last_interaction_;
    uint32_t sleep_after = IsNight() ? kSleepAfterNightMs : kSleepAfterMs;
    if (!asleep_ && idle >= sleep_after) {
        if (activity_ != Activity::kNone) {
            StopActivity(now);
        }
        asleep_ = true;
        bored_ = false;
        return;
    }
    if (asleep_) {
        return;
    }
    bored_ = idle >= kBoredAfterMs;
    if (activity_ == Activity::kNone && !pressing_ && idle >= kFirstActivityAfterMs &&
        static_cast<int32_t>(now - next_activity_) >= 0) {
        uint32_t pick = lv_rand(0, 99);
        Activity next = pick < 40   ? Activity::kGame
                        : pick < 55 ? Activity::kJump
                        : pick < 70 ? Activity::kChase
                        : pick < 85 ? Activity::kRead
                                    : Activity::kSneeze;
        StartActivity(next, now);
    }
}

// ---------------------------------------------------------------------------
// Reactions and activities
// ---------------------------------------------------------------------------

void EyesDisplay::NoteInteraction(uint32_t now, bool startle) {
    bool was_away = asleep_ || bored_ || activity_ != Activity::kNone;
    if (activity_ != Activity::kNone) {
        StopActivity(now);
    }
    asleep_ = false;
    bored_ = false;
    if (startle && was_away) {
        SetReaction("startled", 700, now);
    }
    last_interaction_ = now;
    next_activity_ = now + kFirstActivityAfterMs;
}

void EyesDisplay::SetReaction(const char* name, uint32_t duration_ms, uint32_t now) {
    reaction_ = name;
    reaction_until_ = now + duration_ms;
    queued_reaction_ = nullptr;
}

void EyesDisplay::QueueReaction(const char* name, uint32_t duration_ms) {
    queued_reaction_ = name;
    queued_duration_ = duration_ms;
}

void EyesDisplay::OnTouchEvent(lv_event_t* e) {
    uint32_t now = lv_tick_get();
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED: {
            pressing_ = true;
            if (asleep_ || activity_ != Activity::kNone) {
                // Woken up or interrupted while playing.
                NoteInteraction(now, true);
                break;
            }
            int32_t forgiven = static_cast<int32_t>((now - last_tap_) / kAnnoyanceDecayMs);
            annoyance_ = std::max<int32_t>(0, annoyance_ - forgiven) + 1;
            last_tap_ = now;
            if (annoyance_ >= kAngryTaps) {
                SetReaction("angry", 5000, now);
            } else if (annoyance_ >= kAnnoyedTaps) {
                SetReaction("annoyed", 3500, now);
            } else {
                SetReaction("surprised", 350, now);
                QueueReaction("happy", 2500);
            }
            NoteInteraction(now, false);
            break;
        }
        case LV_EVENT_PRESSING: {
            lv_indev_t* indev = lv_indev_active();
            if (indev == nullptr) {
                break;
            }
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            touch_x_ = std::clamp<int32_t>((point.x - LV_HOR_RES / 2) / 8, -40, 40);
            touch_y_ = std::clamp<int32_t>((point.y - LV_VER_RES / 2) / 8, -14, 14);
            lv_anim_delete(&gaze_x_, nullptr);
            gaze_x_ = 0;
            gaze_y_ = 0;
            ApplyEyes();
            break;
        }
        case LV_EVENT_LONG_PRESSED:
            // Petting calms it down.
            petting_ = true;
            annoyance_ = 0;
            reaction_ = nullptr;
            queued_reaction_ = nullptr;
            break;
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            pressing_ = false;
            touch_x_ = 0;
            touch_y_ = 0;
            if (petting_) {
                petting_ = false;
                SetReaction("happy", 2000, now);
            }
            ApplyEyes();
            break;
        default:
            return;
    }
    RefreshExpression(now);
}

void EyesDisplay::StartActivity(Activity activity, uint32_t now) {
    activity_ = activity;
    activity_start_ = now;
    lv_anim_delete(&gaze_x_, nullptr);
    gaze_x_ = 0;
    gaze_y_ = 0;
    if (activity == Activity::kGame) {
        StartGame(now);
    } else if (activity == Activity::kChase) {
        lv_obj_remove_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    }
}

void EyesDisplay::StopActivity(uint32_t now) {
    if (activity_ == Activity::kGame) {
        lv_obj_add_flag(game_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(face_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    activity_ = Activity::kNone;
    activity_x_ = 0;
    activity_y_ = 0;
    next_activity_ = now + lv_rand(kActivityGapMinMs, kActivityGapMaxMs);
    ApplyEyes();
}

void EyesDisplay::UpdateActivity(uint32_t now) {
    uint32_t elapsed = now - activity_start_;
    switch (activity_) {
        case Activity::kJump: {
            // Three happy hops.
            if (elapsed >= 2400) {
                StopActivity(now);
                return;
            }
            auto angle = static_cast<int16_t>((elapsed % 800) * 180 / 800);
            activity_y_ = -(lv_trigo_sin(angle) * 32 / 32767);
            break;
        }
        case Activity::kChase: {
            // A dot flies around and the eyes follow it.
            if (elapsed >= 6000) {
                StopActivity(now);
                return;
            }
            int32_t dx = Wave(elapsed, 3000, 260);
            int32_t dy = Wave(elapsed, 1500, 40);
            lv_obj_align(dot_, LV_ALIGN_CENTER, dx, kBaseY + dy);
            activity_x_ = dx / 7;
            activity_y_ = dy / 6;
            break;
        }
        case Activity::kRead: {
            // Pretends to read: left to right, line after line.
            if (elapsed >= 4800) {
                StopActivity(now);
                return;
            }
            int32_t line = static_cast<int32_t>(elapsed / 1200);
            int32_t within = static_cast<int32_t>(elapsed % 1200);
            activity_x_ = within < 1000 ? -35 + within * 70 / 1000 : 35 - (within - 1000) * 70 / 200;
            activity_y_ = -8 + line * 5;
            break;
        }
        case Activity::kSneeze:
            if (elapsed >= 2400) {
                StopActivity(now);
                return;
            }
            activity_y_ = elapsed < 1000 ? -static_cast<int32_t>(elapsed * 6 / 1000) : 0;
            break;
        case Activity::kGame:
        case Activity::kNone:
            break;
    }
}

// ---------------------------------------------------------------------------
// Runner game (plays by itself)
// ---------------------------------------------------------------------------

void EyesDisplay::StartGame(uint32_t now) {
    runner_h10_ = 0;
    runner_v10_ = 0;
    game_speed_ = kStartSpeed;
    game_score_ = 0;
    next_gap_ = 0;
    for (auto& obstacle : obstacles_) {
        obstacle.active = false;
        lv_obj_add_flag(obstacle.obj, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(score_label_, "0");
    lv_obj_set_style_bg_color(runner_, lv_color_hex(ExpressionFor("neutral").color), 0);
    lv_obj_set_pos(runner_, kRunnerX, kGroundY - kRunnerH);
    lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(game_, LV_OBJ_FLAG_HIDDEN);
}

void EyesDisplay::UpdateGame(uint32_t now) {
    uint32_t elapsed = now - activity_start_;

    // Runner physics
    if (runner_h10_ > 0 || runner_v10_ > 0) {
        runner_h10_ += runner_v10_;
        runner_v10_ -= kGravity10;
        if (runner_h10_ <= 0) {
            runner_h10_ = 0;
            runner_v10_ = 0;
        }
    }
    bool on_ground = runner_h10_ == 0 && runner_v10_ == 0;
    int32_t runner_height = runner_h10_ / 10;
    int32_t runner_right = kRunnerX + kRunnerW;
    int32_t screen_w = LV_HOR_RES;
    int32_t rightmost = -1000;
    bool crashed = false;

    for (auto& obstacle : obstacles_) {
        if (!obstacle.active) {
            continue;
        }
        obstacle.x -= game_speed_;
        if (obstacle.x + obstacle.w < 0) {
            obstacle.active = false;
            lv_obj_add_flag(obstacle.obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_set_pos(obstacle.obj, obstacle.x, kGroundY - obstacle.h);
        rightmost = std::max(rightmost, obstacle.x + obstacle.w);

        if (!obstacle.scored && obstacle.x + obstacle.w < kRunnerX) {
            obstacle.scored = true;
            game_score_++;
            lv_label_set_text_fmt(score_label_, "%ld", static_cast<long>(game_score_));
            if (game_score_ % 4 == 0 && game_speed_ < kMaxSpeed) {
                game_speed_++;
            }
        }

        // Autoplay: jump when the obstacle is 5-6 ticks away. The gap shrinks by
        // game_speed_ every tick, so this window is hit exactly once per obstacle.
        int32_t gap = obstacle.x - runner_right;
        if (on_ground && !obstacle.will_miss && gap > game_speed_ * 5 &&
            gap <= game_speed_ * 6) {
            runner_v10_ = kJump10;
        }

        bool overlap_x = obstacle.x < runner_right - 4 && obstacle.x + obstacle.w > kRunnerX + 4;
        if (overlap_x && runner_height < obstacle.h - 2) {
            crashed = true;
        }
    }

    // Spawn new obstacles while there is time left.
    if (elapsed + 3000 < kGameDurationMs && screen_w - rightmost >= next_gap_) {
        for (auto& obstacle : obstacles_) {
            if (obstacle.active) {
                continue;
            }
            obstacle.active = true;
            obstacle.scored = false;
            obstacle.will_miss = lv_rand(0, 99) < kMissChancePct;
            obstacle.w = lv_rand(14, 24);
            obstacle.h = lv_rand(22, 44);
            obstacle.x = screen_w;
            lv_obj_set_size(obstacle.obj, obstacle.w, obstacle.h);
            lv_obj_set_pos(obstacle.obj, obstacle.x, kGroundY - obstacle.h);
            lv_obj_remove_flag(obstacle.obj, LV_OBJ_FLAG_HIDDEN);
            next_gap_ = lv_rand(200, 380) + game_speed_ * 8;
            break;
        }
    }

    lv_obj_set_y(runner_, kGroundY - kRunnerH - runner_height);

    if (crashed) {
        SetReaction("dizzy", 3000, now);
        StopActivity(now);
        RefreshExpression(now);
    } else if (elapsed >= kGameDurationMs) {
        SetReaction("confident", 2500, now);  // proud of the run
        StopActivity(now);
        RefreshExpression(now);
    }
}

// ---------------------------------------------------------------------------
// Display overrides
// ---------------------------------------------------------------------------

bool EyesDisplay::SetEmotion(const char* emotion) {
    if (face_ == nullptr) {
        return false;
    }
    DisplayLockGuard lock(this);
    uint32_t now = lv_tick_get();
    server_emotion_ = emotion != nullptr ? emotion : "neutral";
    if (server_emotion_ != "neutral") {
        // The conversation takes over from local reactions.
        reaction_ = nullptr;
        queued_reaction_ = nullptr;
        NoteInteraction(now, false);
    }
    RefreshExpression(now);
    return true;
}

void EyesDisplay::SetStatus(const char* status) {
    CustomLcdDisplay::SetStatus(status);
    if (face_ == nullptr || status == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    uint32_t now = lv_tick_get();
    Mode mode = Mode::kIdle;
    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        mode = Mode::kListening;
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        mode = Mode::kSpeaking;
    }

    if (mode != Mode::kIdle) {
        // Wake word while asleep, bored or playing: startled.
        NoteInteraction(now, mode_ == Mode::kIdle);
    } else if (mode_ != Mode::kIdle) {
        NoteInteraction(now, false);  // conversation just ended
    }
    if (mode != mode_) {
        mode_ = mode;
        if (mode_ != Mode::kIdle) {
            // Focus on the user: center the gaze.
            lv_anim_delete(&gaze_x_, nullptr);
            gaze_x_ = 0;
            gaze_y_ = 0;
        }
    }
    RefreshExpression(now);
    ApplyEyes();
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
    if (question_ != nullptr) {
        auto lvgl_theme = static_cast<LvglTheme*>(theme);
        lv_obj_set_style_text_font(question_, lvgl_theme->large_icon_font()->font(), 0);
    }
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
        if (activity_ != Activity::kNone) {
            StopActivity(lv_tick_get());
        }
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
    }
}
