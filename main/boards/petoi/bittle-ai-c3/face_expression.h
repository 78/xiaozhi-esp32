#ifndef FACE_EXPRESSION_H
#define FACE_EXPRESSION_H

#include <esp_random.h>
#include <lvgl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

struct FaceExpressionConfig {
    lv_color_t background_color = lv_color_black();
    lv_color_t frame_color = lv_color_hex(0x0079A8);
    int frame_border_width = 0;
    int frame_radius = 34;

    int left_ear_x = 0;
    int left_ear_y = 53;
    int left_ear_width = 26;
    int left_ear_height = 90;
    int right_ear_x = 214;
    int right_ear_y = 53;
    int right_ear_width = 26;
    int right_ear_height = 90;
    lv_color_t ear_color = lv_color_hex(0xF3F000);
    float recording_ear_area_scale = 2.6f;

    int eye_center_x = 120;
    int eye_center_y = 110;
    int eye_radius = 56;
    int eye_border_width = 2;
    lv_color_t eye_color = lv_color_hex(0x1CA7D8);
    lv_color_t eye_border_color = lv_color_hex(0x005C84);
    int eye_jitter_range = 20;
    int black_eye_radius = 26;
    lv_color_t black_eye_color = lv_color_black();

    int pupil_radius = 9;
    lv_color_t pupil_color = lv_color_hex(0xE8E8E8);
    lv_color_t pupil_border_color = lv_color_hex(0x0F2A3A);
    int pupil_border_width = 2;
    float focus_eye_scale = 0.8f;

    int mouth_center_x = 120;
    int mouth_center_y = 212;
    int mouth_width = 110;
    int mouth_height = 26;
    int mouth_corner_radius = 8;
    lv_color_t mouth_color = lv_color_hex(0xF31A15);
    lv_color_t mouth_border_color = lv_color_hex(0x005C84);
    int mouth_border_width = 2;
    int mouth_talk_min_width = 60;
    int mouth_talk_max_width = 170;

    uint32_t animation_period_ms = 50;
};

class FaceExpression {
public:
    explicit FaceExpression(lv_obj_t* parent) : parent_(parent) { CreateObjects(); }

    ~FaceExpression() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
        if (root_ != nullptr) {
            lv_obj_del(root_);
            root_ = nullptr;
        }
    }

    static FaceExpressionConfig MakeDefaultConfig(int screen_width, int screen_height) {
        FaceExpressionConfig cfg;
        cfg.left_ear_x = 0;
        cfg.left_ear_y = std::max(0, screen_height / 2 - 87);
        cfg.left_ear_width = 26;
        cfg.left_ear_height = 90;

        cfg.right_ear_width = 26;
        cfg.right_ear_height = 90;
        cfg.right_ear_x = std::max(0, screen_width - cfg.right_ear_width);
        cfg.right_ear_y = cfg.left_ear_y;

        cfg.eye_radius = std::min(screen_width, screen_height) / 4;
        cfg.eye_center_x = screen_width / 2;
        cfg.eye_center_y = screen_height / 2 - 30;
        cfg.eye_jitter_range = 20;
        cfg.black_eye_radius = std::max(10, cfg.eye_radius / 2);

        cfg.pupil_radius = std::max(6, cfg.eye_radius / 6);

        cfg.mouth_width = std::min(112, screen_width / 2);
        cfg.mouth_height = 26;
        cfg.mouth_center_x = screen_width / 2;
        cfg.mouth_center_y = screen_height / 2 + 72;
        cfg.mouth_talk_min_width = std::max(36, cfg.mouth_width - 48);
        cfg.mouth_talk_max_width = std::max(cfg.mouth_talk_min_width, cfg.mouth_width + 58);
        cfg.mouth_corner_radius = 8;

        cfg.frame_radius = std::min(screen_width, screen_height) / 8;
        cfg.animation_period_ms = 50;
        return cfg;
    }

    void Show(const FaceExpressionConfig& config) {
        config_ = config;
        recording_ = false;
        speaking_ = false;
        voice_active_ = false;
        gaze_x_ = 0.0f;
        gaze_y_ = 0.0f;
        gaze_vx_ = 0.0f;
        gaze_vy_ = 0.0f;
        speaking_target_y_ = 0.0f;
        speaking_target_ticks_ = 0;
        idle_target_x_ = 0.0f;
        idle_target_y_ = 0.0f;
        idle_pause_ticks_ = 0;
        idle_need_new_target_ = true;
        manual_look_x_ = 0.0f;
        manual_look_y_ = 0.0f;
        manual_look_ticks_ = 0;
        eye_scale_ = 1.0f;
        target_eye_scale_ = 1.0f;
        blink_scale_ = 1.0f;
        blink_remaining_ = 0;
        blink_phase_ticks_ = 0;
        blink_closing_ = false;
        current_mouth_width_ = static_cast<float>(std::max(2, config_.mouth_width));
        target_mouth_width_ = current_mouth_width_;
        manual_mouth_width_ = -1;
        current_mouth_height_ = std::max(2, config_.mouth_height);
        speech_motion_active_.store(false);
        speech_motion_level_.store(0.0f);

        lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(root_, config_.background_color, 0);
        lv_obj_set_style_border_width(root_, 0, 0);
        lv_obj_set_style_border_color(root_, config_.frame_color, 0);
        lv_obj_set_style_radius(root_, config_.frame_radius, 0);
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);

        ApplyColors();
        ApplyLayout();
        EnsureTimer();
    }

    void SetVisible(bool visible) {
        if (visible)
            lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }

    void SetState(bool recording, bool speaking) {
        bool state_changed = (recording_ != recording) || (speaking_ != speaking);
        recording_ = recording;
        speaking_ = speaking;

        if (!recording_ && !speaking_) {
            voice_active_ = false;
        } else if (speaking_) {
            voice_active_ = false;
            speaking_target_ticks_ = 0;
            speech_motion_active_.store(false);
            speech_motion_level_.store(0.0f);
        }
        if (state_changed) {
            ApplyLayout();
        }
    }

    void SetVoiceActive(bool active) {
        if (voice_active_ == active) {
            return;
        }
        voice_active_ = active;
    }

    void SetSpeechMotion(bool active, float level) {
        speech_motion_active_.store(active);
        speech_motion_level_.store(std::max(0.0f, std::min(1.0f, level)));
    }

    void Look(const std::string& direction) {
        std::string normalized = direction;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) {
                           if (c >= 'A' && c <= 'Z') {
                               return static_cast<char>(c - 'A' + 'a');
                           }
                           return static_cast<char>(c);
                       });

        float target_x = 0.0f;
        float target_y = 0.0f;
        if (normalized == "up") {
            target_y = -0.95f;
        } else if (normalized == "down") {
            target_y = 0.95f;
        } else if (normalized == "left") {
            target_x = -0.95f;
        } else if (normalized == "right") {
            target_x = 0.95f;
        } else if (normalized == "up_left" || normalized == "left_up") {
            target_x = -0.72f;
            target_y = -0.72f;
        } else if (normalized == "up_right" || normalized == "right_up") {
            target_x = 0.72f;
            target_y = -0.72f;
        } else if (normalized == "down_left" || normalized == "left_down") {
            target_x = -0.72f;
            target_y = 0.72f;
        } else if (normalized == "down_right" || normalized == "right_down") {
            target_x = 0.72f;
            target_y = 0.72f;
        } else {
            target_x = 0.0f;
            target_y = 0.0f;
        }

        manual_look_x_ = target_x;
        manual_look_y_ = target_y;
        manual_look_ticks_ = 22;
        idle_pause_ticks_ = 0;
        idle_need_new_target_ = true;
    }

    void Blink(int times) {
        int blink_times = times;
        if (blink_times <= 0) {
            blink_times = RandomInRange(1, 2);
        }
        blink_times = std::max(1, std::min(2, blink_times));
        blink_remaining_ = blink_times;
        blink_closing_ = true;
        blink_phase_ticks_ = 4;
    }

    void SetManualMouthWidth(int width) {
        if (width <= 0) {
            manual_mouth_width_ = -1;
            return;
        }
        manual_mouth_width_ = std::max(2, width);
    }

private:
    lv_obj_t* parent_ = nullptr;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_ear_ = nullptr;
    lv_obj_t* right_ear_ = nullptr;
    lv_obj_t* eye_ = nullptr;
    lv_obj_t* black_eye_ = nullptr;
    lv_obj_t* pupil_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    FaceExpressionConfig config_;
    bool recording_ = false;
    bool speaking_ = false;
    bool voice_active_ = false;
    float gaze_x_ = 0.0f;
    float gaze_y_ = 0.0f;
    float gaze_vx_ = 0.0f;
    float gaze_vy_ = 0.0f;
    float speaking_target_y_ = 0.0f;
    int speaking_target_ticks_ = 0;
    float idle_target_x_ = 0.0f;
    float idle_target_y_ = 0.0f;
    int idle_pause_ticks_ = 0;
    bool idle_need_new_target_ = true;
    float manual_look_x_ = 0.0f;
    float manual_look_y_ = 0.0f;
    int manual_look_ticks_ = 0;
    float eye_scale_ = 1.0f;
    float target_eye_scale_ = 1.0f;
    float blink_scale_ = 1.0f;
    int blink_remaining_ = 0;
    int blink_phase_ticks_ = 0;
    bool blink_closing_ = false;
    float current_mouth_width_ = 0.0f;
    float target_mouth_width_ = 0.0f;
    int manual_mouth_width_ = -1;
    int current_mouth_height_ = 0;
    std::atomic<bool> speech_motion_active_{false};
    std::atomic<float> speech_motion_level_{0.0f};

    static int RandomInRange(int min_value, int max_value) {
        if (max_value <= min_value) {
            return min_value;
        }
        uint32_t value = esp_random();
        int span = max_value - min_value + 1;
        return min_value + static_cast<int>(value % static_cast<uint32_t>(span));
    }

    static float RandomFloat(float min_value, float max_value) {
        if (max_value <= min_value) {
            return min_value;
        }
        constexpr float inv_max = 1.0f / static_cast<float>(UINT32_MAX);
        float normalized = static_cast<float>(esp_random()) * inv_max;
        return min_value + normalized * (max_value - min_value);
    }

    static void TimerCallback(lv_timer_t* timer) {
        auto* self = static_cast<FaceExpression*>(lv_timer_get_user_data(timer));
        if (self != nullptr) {
            self->AnimateStep();
        }
    }

    void CreateObjects() {
        root_ = lv_obj_create(parent_);
        lv_obj_set_style_pad_all(root_, 0, 0);
        lv_obj_set_style_border_width(root_, 0, 0);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
        lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

        left_ear_ = lv_obj_create(root_);
        right_ear_ = lv_obj_create(root_);
        eye_ = lv_obj_create(root_);
        black_eye_ = lv_obj_create(eye_);
        pupil_ = lv_obj_create(black_eye_);
        mouth_ = lv_obj_create(root_);

        for (lv_obj_t* obj : {left_ear_, right_ear_, eye_, black_eye_, pupil_, mouth_}) {
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        }
    }

    void EnsureTimer() {
        uint32_t period = std::max<uint32_t>(30, config_.animation_period_ms);
        if (timer_ == nullptr) {
            timer_ = lv_timer_create(TimerCallback, period, this);
            return;
        }
        lv_timer_set_period(timer_, period);
    }

    void ApplyColors() {
        lv_obj_set_style_bg_color(left_ear_, config_.ear_color, 0);
        lv_obj_set_style_bg_color(right_ear_, config_.ear_color, 0);

        lv_obj_set_style_bg_color(eye_, config_.eye_color, 0);
        lv_obj_set_style_border_width(eye_, config_.eye_border_width, 0);
        lv_obj_set_style_border_color(eye_, config_.eye_border_color, 0);

        lv_obj_set_style_bg_color(black_eye_, config_.black_eye_color, 0);
        lv_obj_set_style_border_width(black_eye_, 0, 0);

        lv_obj_set_style_bg_color(pupil_, config_.pupil_color, 0);
        lv_obj_set_style_border_width(pupil_, config_.pupil_border_width, 0);
        lv_obj_set_style_border_color(pupil_, config_.pupil_border_color, 0);

        lv_obj_set_style_bg_color(mouth_, config_.mouth_color, 0);
        lv_obj_set_style_border_width(mouth_, config_.mouth_border_width, 0);
        lv_obj_set_style_border_color(mouth_, config_.mouth_border_color, 0);
    }

    void ApplyLayout() {
        ApplyEarLayout();
        ApplyEyeLayout();
        ApplyMouthLayout();
    }

    void ApplyEarLayout() {
        float area_scale = recording_ ? std::max(0.1f, config_.recording_ear_area_scale) : 1.0f;
        float linear_scale = std::sqrt(area_scale);

        auto apply_single_ear = [linear_scale, this](lv_obj_t* ear, int x, int y, int width,
                                                     int height, lv_color_t color) {
            int scaled_w = std::max(
                1, static_cast<int>(std::lround(static_cast<float>(width) * linear_scale)));
            int scaled_h = std::max(
                1, static_cast<int>(std::lround(static_cast<float>(height) * linear_scale)));
            int adjusted_x = x - (scaled_w - width) / 2;
            int adjusted_y = y - (scaled_h - height) / 2;

            if (recording_) {
                int min_height_to_top = y + height;
                if (scaled_h < min_height_to_top) {
                    scaled_h = min_height_to_top;
                }
                adjusted_y = 0;
            }

            lv_obj_set_pos(ear, adjusted_x, adjusted_y);
            lv_obj_set_size(ear, scaled_w, scaled_h);
            lv_obj_set_style_radius(ear, 0, 0);
            lv_obj_set_style_bg_color(ear, color, 0);
            lv_obj_set_style_border_width(ear, 0, 0);
        };

        apply_single_ear(left_ear_, config_.left_ear_x, config_.left_ear_y, config_.left_ear_width,
                         config_.left_ear_height, config_.ear_color);
        apply_single_ear(right_ear_, config_.right_ear_x, config_.right_ear_y,
                         config_.right_ear_width, config_.right_ear_height, config_.ear_color);
    }

    void ApplyEyeLayout() {
        float combined_scale = eye_scale_ * blink_scale_;
        int radius = std::max(
            1,
            static_cast<int>(std::lround(static_cast<float>(config_.eye_radius) * combined_scale)));
        int diameter = radius * 2;
        int center_x = config_.eye_center_x;
        int center_y = config_.eye_center_y;
        int x = center_x - radius;
        int y = center_y - radius;

        lv_obj_set_pos(eye_, x, y);
        lv_obj_set_size(eye_, diameter, diameter);
        lv_obj_set_style_radius(eye_, LV_RADIUS_CIRCLE, 0);

        int black_radius =
            std::max(1, static_cast<int>(std::lround(static_cast<float>(config_.black_eye_radius) *
                                                     combined_scale)));
        int black_diameter = black_radius * 2;
        int black_max_offset = std::max(0, radius - black_radius - config_.eye_border_width - 1);
        if (config_.eye_jitter_range > 0) {
            black_max_offset = std::min(black_max_offset, config_.eye_jitter_range);
        }
        int black_offset_x =
            static_cast<int>(std::lround(gaze_x_ * static_cast<float>(black_max_offset)));
        int black_offset_y =
            static_cast<int>(std::lround(gaze_y_ * static_cast<float>(black_max_offset)));
        lv_obj_set_size(black_eye_, black_diameter, black_diameter);
        lv_obj_set_style_radius(black_eye_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(black_eye_, radius - black_radius + black_offset_x,
                       radius - black_radius + black_offset_y);

        int pupil_diameter =
            std::max(2, static_cast<int>(std::lround(static_cast<float>(config_.pupil_radius) *
                                                     combined_scale)) *
                            2);
        int pupil_radius = pupil_diameter / 2;
        int white_max_offset =
            std::max(0, black_radius - pupil_radius - config_.pupil_border_width - 1);
        int white_offset_x =
            static_cast<int>(std::lround(gaze_x_ * static_cast<float>(white_max_offset)));
        int white_offset_y =
            static_cast<int>(std::lround(gaze_y_ * static_cast<float>(white_max_offset)));
        lv_obj_set_size(pupil_, pupil_diameter, pupil_diameter);
        lv_obj_set_style_radius(pupil_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(pupil_, black_radius - pupil_radius + white_offset_x,
                       black_radius - pupil_radius + white_offset_y);
    }

    void ApplyMouthLayout() {
        int width = std::max(2, static_cast<int>(std::lround(current_mouth_width_)));
        int height = std::max(2, current_mouth_height_);
        int x = config_.mouth_center_x - width / 2;
        int y = config_.mouth_center_y - height / 2;

        lv_obj_set_pos(mouth_, x, y);
        lv_obj_set_size(mouth_, width, height);
        lv_obj_set_style_radius(mouth_, config_.mouth_corner_radius, 0);
    }

    void ClampGaze() {
        float radius_sq = gaze_x_ * gaze_x_ + gaze_y_ * gaze_y_;
        if (radius_sq <= 1.0f) {
            return;
        }
        float inv_len = 1.0f / std::sqrt(radius_sq);
        gaze_x_ *= inv_len;
        gaze_y_ *= inv_len;
    }

    void UpdateIdleGaze() {
        if (idle_need_new_target_) {
            constexpr float kTwoPi = 6.28318530718f;
            float angle = RandomFloat(0.0f, kTwoPi);
            float radius = RandomFloat(0.35f, 0.95f);
            idle_target_x_ = std::cos(angle) * radius;
            idle_target_y_ = std::sin(angle) * radius;
            idle_need_new_target_ = false;
        }

        if (idle_pause_ticks_ > 0) {
            idle_pause_ticks_--;
            gaze_vx_ = 0.0f;
            gaze_vy_ = 0.0f;
            gaze_x_ += (idle_target_x_ - gaze_x_) * 0.22f;
            gaze_y_ += (idle_target_y_ - gaze_y_) * 0.22f;
            if (idle_pause_ticks_ == 0) {
                idle_need_new_target_ = true;
            }
            return;
        }

        float dx = idle_target_x_ - gaze_x_;
        float dy = idle_target_y_ - gaze_y_;
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq < 0.0045f) {
            idle_pause_ticks_ = RandomInRange(3, 8);
            gaze_vx_ = 0.0f;
            gaze_vy_ = 0.0f;
            return;
        }

        gaze_vx_ += dx * 0.42f;
        gaze_vy_ += dy * 0.42f;
        gaze_vx_ = std::max(-0.28f, std::min(0.28f, gaze_vx_)) * 0.76f;
        gaze_vy_ = std::max(-0.28f, std::min(0.28f, gaze_vy_)) * 0.76f;
        gaze_x_ += gaze_vx_;
        gaze_y_ += gaze_vy_;
        ClampGaze();
    }

    void PullGazeToCenterFast(float gain) {
        gaze_vx_ = 0.0f;
        gaze_vy_ = 0.0f;
        gaze_x_ += (0.0f - gaze_x_) * gain;
        gaze_y_ += (0.0f - gaze_y_) * gain;
        if (std::fabs(gaze_x_) < 0.003f) {
            gaze_x_ = 0.0f;
        }
        if (std::fabs(gaze_y_) < 0.003f) {
            gaze_y_ = 0.0f;
        }
    }

    void UpdateManualLook() {
        float dx = manual_look_x_ - gaze_x_;
        float dy = manual_look_y_ - gaze_y_;
        gaze_vx_ += dx * 0.55f;
        gaze_vy_ += dy * 0.55f;
        gaze_vx_ = std::max(-0.32f, std::min(0.32f, gaze_vx_)) * 0.75f;
        gaze_vy_ = std::max(-0.32f, std::min(0.32f, gaze_vy_)) * 0.75f;
        gaze_x_ += gaze_vx_;
        gaze_y_ += gaze_vy_;
        ClampGaze();
        if (manual_look_ticks_ > 0) {
            manual_look_ticks_--;
        }
    }

    void UpdateBlinkAnimation() {
        if (blink_remaining_ <= 0) {
            blink_scale_ += (1.0f - blink_scale_) * 0.25f;
            if (std::fabs(1.0f - blink_scale_) < 0.002f) {
                blink_scale_ = 1.0f;
            }
            return;
        }

        if (blink_closing_) {
            // Keep shrink ratio moderate to avoid harsh flicker.
            blink_scale_ += (0.62f - blink_scale_) * 0.46f;
            if (--blink_phase_ticks_ <= 0) {
                blink_closing_ = false;
                blink_phase_ticks_ = 4;
            }
        } else {
            blink_scale_ += (1.0f - blink_scale_) * 0.42f;
            if (--blink_phase_ticks_ <= 0) {
                blink_remaining_--;
                if (blink_remaining_ > 0) {
                    blink_closing_ = true;
                    blink_phase_ticks_ = RandomInRange(3, 4);
                } else {
                    blink_scale_ = 1.0f;
                }
            }
        }
    }

    void UpdateSpeakingGaze() {
        if (speaking_target_ticks_ <= 0) {
            speaking_target_y_ = RandomFloat(-0.35f, 0.35f);
            speaking_target_ticks_ = RandomInRange(4, 9);
        }
        speaking_target_ticks_--;

        gaze_vx_ += (0.0f - gaze_x_) * 0.20f;
        gaze_vx_ *= 0.72f;
        gaze_vy_ += (speaking_target_y_ - gaze_y_) * 0.17f;
        gaze_vy_ *= 0.78f;

        gaze_x_ += gaze_vx_;
        gaze_y_ += gaze_vy_;
        gaze_x_ = std::max(-0.20f, std::min(0.20f, gaze_x_));
        gaze_y_ = std::max(-0.45f, std::min(0.45f, gaze_y_));
    }

    void UpdateSpeakingMouth(bool speech_active, float speech_level) {
        if (!speech_active) {
            if (manual_mouth_width_ > 0) {
                float manual_width = static_cast<float>(manual_mouth_width_);
                current_mouth_width_ += (manual_width - current_mouth_width_) * 0.35f;
            } else {
                RestoreMouthWidth();
            }
            return;
        }
        int min_w = std::max(2, config_.mouth_talk_min_width);
        int max_w = std::max(min_w, config_.mouth_talk_max_width);
        float normalized = std::max(0.0f, std::min(1.0f, speech_level));
        float boosted = std::min(1.0f, normalized * 1.25f + 0.08f);
        float jitter = RandomFloat(-0.10f, 0.10f) * (0.25f + boosted);
        float mapped = std::max(0.0f, std::min(1.0f, boosted + jitter));
        target_mouth_width_ =
            static_cast<float>(min_w) + mapped * static_cast<float>(max_w - min_w);
        current_mouth_width_ += (target_mouth_width_ - current_mouth_width_) * 0.62f;
    }

    void RestoreMouthWidth() {
        float base_width = static_cast<float>(
            std::max(2, manual_mouth_width_ > 0 ? manual_mouth_width_ : config_.mouth_width));
        target_mouth_width_ = base_width;
        current_mouth_width_ += (base_width - current_mouth_width_) * 0.25f;
        if (std::fabs(base_width - current_mouth_width_) < 0.2f) {
            current_mouth_width_ = base_width;
        }
    }

    void AnimateStep() {
        if (root_ == nullptr || lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        float prev_gaze_x = gaze_x_;
        float prev_gaze_y = gaze_y_;
        float prev_scale = eye_scale_;
        float prev_blink_scale = blink_scale_;
        float prev_mouth_width = current_mouth_width_;
        bool speech_active = speech_motion_active_.load();
        float speech_level = speech_motion_level_.load();

        if (manual_look_ticks_ > 0) {
            target_eye_scale_ = 1.0f;
            UpdateManualLook();
            if (speaking_) {
                UpdateSpeakingMouth(speech_active, speech_level);
            } else {
                RestoreMouthWidth();
            }
        } else if (speaking_) {
            target_eye_scale_ = 1.0f;
            UpdateSpeakingGaze();
            UpdateSpeakingMouth(speech_active, speech_level);
        } else if (recording_ && voice_active_) {
            target_eye_scale_ = std::max(0.2f, config_.focus_eye_scale);
            PullGazeToCenterFast(0.72f);
            RestoreMouthWidth();
        } else if (recording_) {
            target_eye_scale_ = 1.0f;
            PullGazeToCenterFast(0.44f);
            RestoreMouthWidth();
        } else {
            target_eye_scale_ = 1.0f;
            UpdateIdleGaze();
            RestoreMouthWidth();
        }

        eye_scale_ += (target_eye_scale_ - eye_scale_) * 0.20f;
        if (std::fabs(eye_scale_ - target_eye_scale_) < 0.003f) {
            eye_scale_ = target_eye_scale_;
        }
        UpdateBlinkAnimation();

        bool eye_changed = std::fabs(prev_gaze_x - gaze_x_) >= 0.001f ||
                           std::fabs(prev_gaze_y - gaze_y_) >= 0.001f ||
                           std::fabs(prev_scale - eye_scale_) >= 0.001f ||
                           std::fabs(prev_blink_scale - blink_scale_) >= 0.001f;
        bool mouth_changed = std::fabs(prev_mouth_width - current_mouth_width_) >= 0.2f;
        if (!eye_changed && !mouth_changed) {
            return;
        }
        if (eye_changed) {
            ApplyEyeLayout();
        }
        if (mouth_changed) {
            ApplyMouthLayout();
        }
    }
};

#endif  // FACE_EXPRESSION_H
