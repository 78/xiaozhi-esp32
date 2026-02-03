#include "face_rabbit.h"
#include <esp_log.h>

#define TAG "RabbitFace"

RabbitFace::RabbitFace() : VectorFace() {
}

RabbitFace::~RabbitFace() {
}

void RabbitFace::Create(lv_obj_t* parent) {
    if (is_created_) return;

    parent_ = parent;
    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t white = lv_color_hex(0xFFFFFF);

    // Create container
    face_container_ = lv_obj_create(parent);
    lv_obj_set_size(face_container_, kDisplayWidth, kDisplayHeight);
    lv_obj_center(face_container_);
    lv_obj_set_style_bg_opa(face_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_container_, 0, 0);
    lv_obj_set_style_pad_all(face_container_, 0, 0);
    lv_obj_set_scrollbar_mode(face_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(face_container_, LV_OBJ_FLAG_CLICKABLE);

    // Eyes (circles with pupils)
    left_eye_ = CreateCircle(face_container_, kCenterX - kEyeSpacing, kEyeY, kEyeRadius, black);
    right_eye_ = CreateCircle(face_container_, kCenterX + kEyeSpacing, kEyeY, kEyeRadius, black);

    // Pupils (white highlight) - slightly offset for cute look
    left_pupil_ = CreateCircle(face_container_, kCenterX - kEyeSpacing - 3, kEyeY - 3, 5, white);
    right_pupil_ = CreateCircle(face_container_, kCenterX + kEyeSpacing - 3, kEyeY - 3, 5, white);

    // Closed eyes
    left_eye_closed_ = CreateOval(face_container_, kCenterX - kEyeSpacing, kEyeY, kEyeRadius * 2, 3, black);
    right_eye_closed_ = CreateOval(face_container_, kCenterX + kEyeSpacing, kEyeY, kEyeRadius * 2, 3, black);
    lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);

    // Small pink nose (triangle-ish oval)
    nose_ = CreateOval(face_container_, kCenterX, kNoseY, 14, 10, black);

    // Whiskers (horizontal lines)
    for (int i = 0; i < 3; i++) {
        int y_offset = (i - 1) * 10;  // -10, 0, 10
        // Left whiskers
        whiskers_left_[i] = CreateOval(face_container_, kCenterX - 55, kNoseY + y_offset, 35, 2, black);
        // Right whiskers
        whiskers_right_[i] = CreateOval(face_container_, kCenterX + 55, kNoseY + y_offset, 35, 2, black);
    }

    // Mouth variations
    mouth_smile_ = CreateArc(face_container_, kCenterX, kMouthY - 15, 30, 24, 200, 340, black);

    mouth_frown_ = CreateArc(face_container_, kCenterX, kMouthY + 15, 30, 24, 20, 160, black);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);

    mouth_open_ = CreateOval(face_container_, kCenterX, kMouthY, 25, 18, black);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);

    mouth_line_ = CreateOval(face_container_, kCenterX, kMouthY, 30, 3, black);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);

    // Buck teeth (two small rectangles under smile)
    teeth_ = CreateOval(face_container_, kCenterX, kMouthY + 5, 16, 10, black);

    is_created_ = true;
    ESP_LOGI(TAG, "Rabbit face created");
}

void RabbitFace::Update() {
    if (!is_created_) return;
    UpdateEyes();
    UpdateMouth();
}

void RabbitFace::UpdateEyes() {
    if (!is_created_) return;

    bool show_open = (eye_state_ == EyeState::kOpen || eye_state_ == EyeState::kHalfOpen);

    if (show_open) {
        lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(left_eye_closed_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_eye_closed_, LV_OBJ_FLAG_HIDDEN);
    }
}

void RabbitFace::UpdateMouth() {
    if (!is_created_) return;

    lv_obj_add_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_frown_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(teeth_, LV_OBJ_FLAG_HIDDEN);

    switch (mouth_state_) {
        case MouthState::kSmile:
            lv_obj_remove_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(teeth_, LV_OBJ_FLAG_HIDDEN);
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

void RabbitFace::Animate(int frame) {
    if (!is_created_) return;

    // Blinking
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
        if (speak_counter_ % 5 == 0) {
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
