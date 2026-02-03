#include "face_heart.h"
#include <esp_log.h>
#include <cmath>

#define TAG "HeartFace"

HeartFace::HeartFace() : VectorFace() {
}

HeartFace::~HeartFace() {
}

void HeartFace::Create(lv_obj_t* parent) {
    if (is_created_) return;

    parent_ = parent;
    lv_color_t red = lv_color_hex(0xE63946);  // Nice heart red

    // Create container
    face_container_ = lv_obj_create(parent);
    lv_obj_set_size(face_container_, kDisplayWidth, kDisplayHeight);
    lv_obj_center(face_container_);
    lv_obj_set_style_bg_opa(face_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_container_, 0, 0);
    lv_obj_set_style_pad_all(face_container_, 0, 0);
    lv_obj_set_scrollbar_mode(face_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(face_container_, LV_OBJ_FLAG_CLICKABLE);

    // Heart shape parameters - medium sized heart centered on screen
    // The heart is centered at (kCenterX, kCenterY)
    int heart_size = 45;  // Base size for heart (radius of top bumps)
    int center_y_offset = -5;  // Shift heart up slightly

    // Two overlapping circles at top form the bumps
    // Position them so they touch/overlap at center
    int bump_spacing = heart_size - 5;  // Overlap the circles
    int bump_y = kCenterY + center_y_offset - 5;  // Top bumps position

    // Left bump (circle)
    heart_left_ = CreateCircle(face_container_, kCenterX - bump_spacing, bump_y, heart_size, red);

    // Right bump (circle)
    heart_right_ = CreateCircle(face_container_, kCenterX + bump_spacing, bump_y, heart_size, red);

    // Bottom point - use rotated square to create the pointed tip
    // Create a large rectangle that will be rotated 45 degrees to form diamond/point
    int point_size = heart_size * 2;  // Size of the rotated square
    int point_y = kCenterY + center_y_offset + 20;  // Position below the bumps

    heart_bottom_ = lv_obj_create(face_container_);
    lv_obj_set_size(heart_bottom_, point_size, point_size);
    lv_obj_set_style_bg_color(heart_bottom_, red, 0);
    lv_obj_set_style_bg_opa(heart_bottom_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(heart_bottom_, 0, 0);
    lv_obj_set_style_radius(heart_bottom_, 4, 0);  // Slight rounding
    lv_obj_align(heart_bottom_, LV_ALIGN_CENTER, 0, point_y - kCenterY + 15);
    // Rotate 45 degrees (4500 = 45.00 degrees in LVGL)
    lv_obj_set_style_transform_rotation(heart_bottom_, 450, 0);
    // Set pivot to center for proper rotation
    lv_obj_set_style_transform_pivot_x(heart_bottom_, point_size / 2, 0);
    lv_obj_set_style_transform_pivot_y(heart_bottom_, point_size / 2, 0);
    lv_obj_remove_flag(heart_bottom_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(heart_bottom_, LV_OBJ_FLAG_CLICKABLE);

    // Center fill - rectangle to connect bumps and hide gaps
    heart_center_ = lv_obj_create(face_container_);
    lv_obj_set_size(heart_center_, bump_spacing * 2 + 20, heart_size + 10);
    lv_obj_set_style_bg_color(heart_center_, red, 0);
    lv_obj_set_style_bg_opa(heart_center_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(heart_center_, 0, 0);
    lv_obj_set_style_radius(heart_center_, 0, 0);
    lv_obj_align(heart_center_, LV_ALIGN_CENTER, 0, bump_y - kCenterY + 10);
    lv_obj_remove_flag(heart_center_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(heart_center_, LV_OBJ_FLAG_CLICKABLE);

    is_created_ = true;
    current_scale_ = 1.0f;
    pulse_phase_ = 0;
    ESP_LOGI(TAG, "Heart face created - medium heart with slow pulse");
}

void HeartFace::Update() {
    if (!is_created_) return;
    // Heart doesn't change expression, just pulses
}

void HeartFace::Animate(int frame) {
    if (!is_created_) return;

    // SLOW gentle heartbeat - like a real resting heart (~60 BPM = 1 beat per second)
    // At 30fps, 60 frames = 2 seconds for a full cycle (30 BPM for gentle effect)
    pulse_phase_ = frame % kPulseCycle;

    // Realistic heartbeat: quick contraction, slow relaxation
    // Two-phase beat like "lub-dub"
    float phase_ratio = (float)pulse_phase_ / kPulseCycle;
    float scale;

    if (phase_ratio < 0.15f) {
        // First beat (lub) - quick expansion
        float t = phase_ratio / 0.15f;
        scale = kPulseMin + (kPulseMax - kPulseMin) * sinf(t * 3.14159f / 2.0f);
    } else if (phase_ratio < 0.25f) {
        // Brief pause and slight contraction
        float t = (phase_ratio - 0.15f) / 0.10f;
        scale = kPulseMax - (kPulseMax - kPulseMin) * 0.3f * t;
    } else if (phase_ratio < 0.40f) {
        // Second beat (dub) - smaller expansion
        float t = (phase_ratio - 0.25f) / 0.15f;
        float mid = kPulseMax - (kPulseMax - kPulseMin) * 0.3f;
        scale = mid + (kPulseMax - mid) * 0.7f * sinf(t * 3.14159f / 2.0f);
    } else {
        // Long relaxation period
        float t = (phase_ratio - 0.40f) / 0.60f;
        float start = kPulseMax - (kPulseMax - kPulseMin) * 0.3f * 0.3f;
        scale = start + (kPulseMin - start) * (1.0f - cosf(t * 3.14159f)) * 0.5f;
    }

    // Only update if scale changed enough to be visible
    if (fabsf(scale - current_scale_) > 0.003f) {
        int scale_256 = (int)(256.0f * scale);  // LVGL uses 256 = 100%

        if (heart_left_) {
            lv_obj_set_style_transform_scale(heart_left_, scale_256, 0);
        }
        if (heart_right_) {
            lv_obj_set_style_transform_scale(heart_right_, scale_256, 0);
        }
        if (heart_center_) {
            lv_obj_set_style_transform_scale(heart_center_, scale_256, 0);
        }
        if (heart_bottom_) {
            lv_obj_set_style_transform_scale(heart_bottom_, scale_256, 0);
        }

        current_scale_ = scale;
    }
}
