#include "face_bear.h"
#include <esp_log.h>

#define TAG "BearFace"

BearFace::BearFace() : VectorFace() {
}

BearFace::~BearFace() {
    // Parent destructor calls Destroy()
}

void BearFace::Create(lv_obj_t* parent) {
    if (is_created_) {
        return;
    }

    parent_ = parent;
    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t face_color = lv_color_hex(0x000000);

    // Create container for face elements
    face_container_ = lv_obj_create(parent);
    lv_obj_set_size(face_container_, kDisplayWidth, kDisplayHeight);
    lv_obj_center(face_container_);
    lv_obj_set_style_bg_opa(face_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_container_, 0, 0);
    lv_obj_set_style_pad_all(face_container_, 0, 0);
    lv_obj_set_scrollbar_mode(face_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(face_container_, LV_OBJ_FLAG_CLICKABLE);

    // Create eyes (open state)
    left_eye_ = CreateCircle(face_container_, kCenterX - kEyeSpacing, kEyeY, kEyeRadius, face_color);
    right_eye_ = CreateCircle(face_container_, kCenterX + kEyeSpacing, kEyeY, kEyeRadius, face_color);

    // Create eyes (closed state) - thin horizontal lines
    left_eye_closed_ = CreateOval(face_container_, kCenterX - kEyeSpacing, kEyeY, kEyeRadius * 2, 4, face_color);
    right_eye_closed_ = CreateOval(face_container_, kCenterX + kEyeSpacing, kEyeY, kEyeRadius * 2, 4, face_color);
    lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);

    // Create nose (oval)
    nose_ = CreateOval(face_container_, kCenterX, kNoseY, kNoseWidth, kNoseHeight, black);

    // Create mouth variations
    // Smile - arc curves DOWN (happy U shape) - angles 0-180 draw bottom half
    mouth_smile_ = CreateArc(face_container_, kCenterX, kMouthY - 20, 35, 28, 0, 180, black);

    // Frown - arc curves UP (sad upside-down U) - angles 180-360 draw top half
    mouth_frown_ = CreateArc(face_container_, kCenterX, kMouthY + 5, 35, 28, 180, 360, black);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);

    // Open mouth (excited/speaking) - oval
    mouth_open_ = CreateOval(face_container_, kCenterX, kMouthY, 30, 20, black);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);

    // Neutral mouth - straight line
    mouth_line_ = CreateOval(face_container_, kCenterX, kMouthY, 40, 4, black);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);

    is_created_ = true;
    ESP_LOGI(TAG, "Bear face created");
}

void BearFace::Update() {
    if (!is_created_) return;

    UpdateEyes();
    UpdateMouth();
}

void BearFace::UpdateEyes() {
    if (!is_created_) return;

    bool show_open = true;

    switch (eye_state_) {
        case EyeState::kOpen:
            show_open = true;
            break;
        case EyeState::kHalfOpen:
            // Show slightly smaller eyes
            show_open = true;
            break;
        case EyeState::kClosed:
            show_open = false;
            break;
        case EyeState::kSquint:
            // Show closed eyes for squint
            show_open = false;
            break;
    }

    // Toggle eye visibility
    if (show_open) {
        lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    }
}

void BearFace::UpdateMouth() {
    if (!is_created_) return;

    // Hide all mouth variants first
    lv_obj_add_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);

    // Show appropriate mouth
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

void BearFace::Animate(int frame) {
    if (!is_created_) return;

    // Handle blinking
    int blink_phase = frame % kBlinkInterval;

    // Start blink
    if (blink_phase == 0 && eye_state_ == EyeState::kOpen) {
        eye_state_ = EyeState::kClosed;
        UpdateEyes();
    }
    // End blink
    else if (blink_phase == kBlinkDuration && current_emotion_ != FaceEmotion::kSleepy) {
        // Restore eye state based on emotion
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

    // Handle speaking mouth animation
    if (mouth_state_ == MouthState::kSpeaking) {
        speak_counter_++;
        if (speak_counter_ % 6 == 0) {  // Toggle every ~0.2s
            // Alternate between open and slightly closed
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
