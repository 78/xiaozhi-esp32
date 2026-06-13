#pragma once

#include <lvgl.h>
#include <stdint.h>

enum class FaceState { Idle, Listening, Speaking };

class FaceEngine {
public:
    void Init(lv_obj_t* parent);
    void SetState(FaceState state);
    void Update();

private:
    void IdleBehavior(int base_eye_height);
    void ListeningBehavior(int base_eye_height);
    void SpeakingBehavior(int eye_height);

    lv_obj_t* container_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* mouth_ = nullptr;

    FaceState state_ = FaceState::Idle;

    int eye_size_ = 30;

    int idle_move_offset_x_ = 0;
    int idle_move_offset_y_ = 0;

    int blink_phase_ = 0;

    uint32_t speak_last_update_ = 0;
    int speak_mouth_target_ = 4;
    int speak_mouth_current_ = 4;
};