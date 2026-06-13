#include "face_engine.h"
#include "robot_eyes_assets.h" 
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

    // lv_image_create in plaats van lv_img_create voor LVGL v9
    left_eye_ = lv_image_create(container_);
    right_eye_ = lv_image_create(container_);
    mouth_ = lv_obj_create(container_);

    lv_obj_set_style_bg_color(mouth_, lv_color_white(), 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);
    lv_obj_set_style_radius(mouth_, 4, 0);

    // We geven expliciet de index [0] mee voor de array pointer conversie
    lv_image_set_src(left_eye_, idle_frames[0]);
    lv_image_set_src(right_eye_, idle_frames[0]);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_timer_create(
        [](lv_timer_t* t) {
            auto face = static_cast<FaceEngine*>(lv_timer_get_user_data(t));
            face->Update();
        },
        60, this);
}

void FaceEngine::SetState(FaceState state) { 
    if (state_ != state) {
        state_ = state; 
        current_frame_index_ = 0; 
    }
}

void FaceEngine::IdleBehavior(int base_eye_height) {
    if (rand() % 40 == 0) {
        idle_move_offset_x_ = (rand() % 5) - 2;
        idle_move_offset_y_ = (rand() % 3) - 1;
    }

    int frame_index = 0;
    if (blink_phase_ > 0 && blink_phase_ <= 3) {
        frame_index = blink_phase_ - 1; 
    }

    lv_image_set_src(left_eye_, idle_frames[frame_index]);
    lv_image_set_src(right_eye_, idle_frames[frame_index]);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X + idle_move_offset_x_,
                 -EYE_OFFSET_Y + idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X + idle_move_offset_x_,
                 -EYE_OFFSET_Y + idle_move_offset_y_);

    lv_obj_set_size(mouth_, 20, 7);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 17 + idle_move_offset_y_);
}

void FaceEngine::ListeningBehavior(int base_eye_height) {
    lv_image_set_src(left_eye_, idle_frames[0]);
    lv_image_set_src(right_eye_, idle_frames[0]);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_obj_set_size(mouth_, 10, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, -4, 17);
}

void FaceEngine::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    lv_image_set_src(left_eye_, idle_frames[0]);
    lv_image_set_src(right_eye_, idle_frames[0]);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    if (now - speak_last_update_ > 90 + (rand() % 60)) {
        speak_last_update_ = now;
        int r = rand() % 100;
        if (r < 20) speak_mouth_target_ = 2;
        else if (r < 50) speak_mouth_target_ = 6;
        else if (r < 80) speak_mouth_target_ = 10;
        else speak_mouth_target_ = 16;
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

    if (state_ == FaceState::Idle) {
        if (blink_phase_ == 0) {
            if (rand() % 120 == 0) {
                blink_phase_ = 1;
            }
        } else {
            blink_phase_ = (blink_phase_ + 1) % 4;
        }
    }

    switch (state_) {
        case FaceState::Idle:
            IdleBehavior(eye_size_);
            break;
        case FaceState::Listening:
            ListeningBehavior(eye_size_);
            break;
        case FaceState::Speaking:
            SpeakingBehavior(eye_size_);
            break;

        case FaceState::Thinking:
            if (current_frame_index_ >= THINKING_FRAME_COUNT) {
                current_frame_index_ = 0; 
            }
            lv_image_set_src(left_eye_, thinking_frames[current_frame_index_]);
            lv_image_set_src(right_eye_, thinking_frames[current_frame_index_]);
            lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
            lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);
            current_frame_index_++;

            lv_obj_set_size(mouth_, 12, 3);
            lv_obj_set_style_radius(mouth_, 2, 0);
            lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 17);
            break;

        case FaceState::Focus:
            if (current_frame_index_ >= FOCUS_FRAME_COUNT) {
                state_ = FaceState::Idle;
                current_frame_index_ = 0;
                break;
            }
            lv_image_set_src(left_eye_, focus_frames[current_frame_index_]);
            lv_image_set_src(right_eye_, focus_frames[current_frame_index_]);
            lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
            lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);
            current_frame_index_++;

            lv_obj_set_size(mouth_, 16, 2);
            lv_obj_set_style_radius(mouth_, 0, 0);
            lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 17);
            break;
    }
}
