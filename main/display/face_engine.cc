#include "face_engine.h"
#include <stdlib.h>

#include <esp_log.h>

#define TAG "Face Engine"

#define EYE_OFFSET_X 5
#define EYE_OFFSET_Y 4

void FaceEngine::Init(lv_obj_t* parent) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 128, 64);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);

    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);
    mouth_ = lv_obj_create(container_);

    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mouth_, lv_color_white(), 0);

    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);

    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);
    lv_obj_set_style_radius(mouth_, 4, 0);

    lv_obj_set_size(left_eye_, eye_size_, eye_size_);
    lv_obj_set_size(right_eye_, eye_size_, eye_size_);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_timer_create(
        [](lv_timer_t* t) {
            auto face = static_cast<FaceEngine*>(lv_timer_get_user_data(t));
            face->Update();
        },
        60, this);
}

void FaceEngine::SetState(FaceState state) { state_ = state; }

void FaceEngine::IdleBehavior(int base_eye_height) {
    if (rand() % 40 == 0) {
        idle_move_offset_x_ = (rand() % 5) - 2;
        idle_move_offset_y_ = (rand() % 3) - 1;
    }

    int eye_h = base_eye_height;
    int eye_w = eye_h * 0.65;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, - eye_size_ + idle_move_offset_x_,
                 -5 + idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + idle_move_offset_x_,
                 -5 + idle_move_offset_y_);

    lv_obj_set_size(mouth_, 20, 7);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 17 + idle_move_offset_y_);
}

void FaceEngine::ListeningBehavior(int base_eye_height) {
    int right_h = base_eye_height;
    int right_w = right_h * 0.65;

    int left_h = base_eye_height - 2;
    int left_w = left_h * 0.65;

    lv_obj_set_size(left_eye_, left_w, left_h);
    lv_obj_set_size(right_eye_, right_w, right_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_obj_set_size(mouth_, 10, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, -4, 17);
}

void FaceEngine::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    lv_obj_set_size(left_eye_, eye_height * 0.65, eye_height);
    lv_obj_set_size(right_eye_, eye_height * 0.65, eye_height);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    if (now - speak_last_update_ > 90 + (rand() % 60)) {
        speak_last_update_ = now;

        int r = rand() % 100;

        if (r < 20)
            speak_mouth_target_ = 2;
        else if (r < 50)
            speak_mouth_target_ = 6;
        else if (r < 80)
            speak_mouth_target_ = 10;
        else
            speak_mouth_target_ = 16;
    }

    if (speak_mouth_current_ < speak_mouth_target_)
        speak_mouth_current_ += 2;
    else if (speak_mouth_current_ > speak_mouth_target_)
        speak_mouth_current_ -= 2;

    lv_obj_set_size(mouth_, 15, speak_mouth_current_);
    lv_obj_set_style_radius(mouth_, 8, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 18);
}

void FaceEngine::Update() {
    if (!container_)
        return;

    if (blink_phase_ == 0) {
        if (rand() % 120 == 0) {
            blink_phase_ = 1;
        }
    }

    int eye_height = eye_size_;

    switch (blink_phase_) {
        case 1:
            eye_height = 4;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 1;
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = 6;
            blink_phase_ = 0;
            break;
        default:
            break;
    }

    switch (state_) {
        case FaceState::Idle:
            IdleBehavior(eye_height);
            break;
        case FaceState::Listening:
            ListeningBehavior(eye_height);
            break;
        case FaceState::Speaking:
            SpeakingBehavior(eye_height);
            break;
    }

    /*
    ESP_LOGI(
        TAG,
        "Face update: state=%d, eye_height=%d, blink_phase=%d, mouth_target=%d, mouth_current=%d",
        static_cast<int>(state_), eye_height, blink_phase_, speak_mouth_target_,
        speak_mouth_current_);
        */
}