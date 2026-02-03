#include "face_cat.h"
#include <esp_log.h>

#define TAG "CatFace"

CatFace::CatFace() : VectorFace() {
}

CatFace::~CatFace() {
}

void CatFace::Create(lv_obj_t* parent) {
    if (is_created_) return;

    parent_ = parent;
    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t green = lv_color_hex(0x00AA00);  // Cat eye color

    // Create container
    face_container_ = lv_obj_create(parent);
    lv_obj_set_size(face_container_, kDisplayWidth, kDisplayHeight);
    lv_obj_center(face_container_);
    lv_obj_set_style_bg_opa(face_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_container_, 0, 0);
    lv_obj_set_style_pad_all(face_container_, 0, 0);
    lv_obj_set_scrollbar_mode(face_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(face_container_, LV_OBJ_FLAG_CLICKABLE);

    // Simple round eyes (like bear but slightly oval for cat look)
    left_eye_ = CreateCircle(face_container_, kCenterX - kEyeSpacing, kEyeY, 18, black);
    right_eye_ = CreateCircle(face_container_, kCenterX + kEyeSpacing, kEyeY, 18, black);

    // No pupils - keep it simple and clean
    left_pupil_ = nullptr;
    right_pupil_ = nullptr;

    // Closed eyes (thin horizontal lines)
    left_eye_closed_ = CreateOval(face_container_, kCenterX - kEyeSpacing, kEyeY, 36, 4, black);
    right_eye_closed_ = CreateOval(face_container_, kCenterX + kEyeSpacing, kEyeY, 36, 4, black);
    lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);

    // Small oval nose
    nose_ = CreateOval(face_container_, kCenterX, kNoseY, 14, 10, black);

    // Whiskers - 3 on each side
    for (int i = 0; i < 3; i++) {
        int y_offset = (i - 1) * 10;
        // Left whiskers (positioned to the left of nose)
        whiskers_left_[i] = CreateOval(face_container_, kCenterX - 55, kNoseY + y_offset, 35, 2, black);
        // Right whiskers (positioned to the right of nose)
        whiskers_right_[i] = CreateOval(face_container_, kCenterX + 55, kNoseY + y_offset, 35, 2, black);
    }

    // Mouth variations
    // Smile - curves DOWN (happy U shape) - LVGL: 0-180 is bottom half
    mouth_smile_ = CreateArc(face_container_, kCenterX, kMouthY - 15, 30, 24, 0, 180, black);

    // Frown - curves UP (sad) - LVGL: 180-360 is top half
    mouth_frown_ = CreateArc(face_container_, kCenterX, kMouthY + 10, 30, 24, 180, 360, black);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);

    mouth_open_ = CreateOval(face_container_, kCenterX, kMouthY, 28, 20, black);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);

    mouth_line_ = CreateOval(face_container_, kCenterX, kMouthY, 35, 3, black);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);

    is_created_ = true;
    ESP_LOGI(TAG, "Cat face created");
}

void CatFace::Update() {
    if (!is_created_) return;
    UpdateEyes();
    UpdateMouth();
}

void CatFace::UpdateEyes() {
    if (!is_created_) return;

    bool show_open = (eye_state_ == EyeState::kOpen || eye_state_ == EyeState::kHalfOpen);

    if (show_open) {
        if (left_eye_) lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_) lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        if (left_pupil_) lv_obj_remove_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_remove_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (left_eye_closed_) lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_closed_) lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (left_eye_) lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_) lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        if (left_pupil_) lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (left_eye_closed_) lv_obj_remove_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_closed_) lv_obj_remove_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    }
}

void CatFace::UpdateMouth() {
    if (!is_created_) return;

    lv_obj_add_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);

    switch (mouth_state_) {
        case MouthState::kSmile:
            lv_obj_remove_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
            break;
        case MouthState::kFrown:
            lv_obj_remove_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);
            break;
        case MouthState::kOpen:
        case MouthState::kSpeaking:
            lv_obj_remove_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
            break;
        case MouthState::kClosed:
        default:
            lv_obj_remove_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

void CatFace::Animate(int frame) {
    if (!is_created_) return;

    // Blinking - cats blink slowly
    int blink_phase = frame % kBlinkInterval;
    if (blink_phase == 0 && eye_state_ == EyeState::kOpen) {
        eye_state_ = EyeState::kClosed;
        UpdateEyes();
    } else if (blink_phase == kBlinkDuration && current_emotion_ != FaceEmotion::kSleepy) {
        switch (current_emotion_) {
            case FaceEmotion::kThinking:
                eye_state_ = EyeState::kSquint;
                break;
            case FaceEmotion::kSad:
                eye_state_ = EyeState::kHalfOpen;
                break;
            case FaceEmotion::kSleepy:
                eye_state_ = EyeState::kClosed;
                break;
            default:
                eye_state_ = EyeState::kOpen;
                break;
        }
        UpdateEyes();
    }

    // Speaking animation
    if (mouth_state_ == MouthState::kSpeaking) {
        speak_counter_++;
        if (speak_counter_ % 6 == 0) {
            if (lv_obj_has_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_remove_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}
