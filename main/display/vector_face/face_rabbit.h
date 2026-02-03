#ifndef FACE_RABBIT_H
#define FACE_RABBIT_H

#include "vector_face.h"

/**
 * Rabbit face - cute bunny design.
 * Features: Circle eyes, small nose, whiskers, buck teeth hint
 */
class RabbitFace : public VectorFace {
public:
    RabbitFace();
    ~RabbitFace() override;

    void Create(lv_obj_t* parent) override;
    void Update() override;
    void Animate(int frame) override;

    const char* GetName() const override { return "Rabbit"; }
    const char* GetId() const override { return "rabbit"; }

private:
    void UpdateEyes();
    void UpdateMouth();

    // Eye objects
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_eye_closed_ = nullptr;
    lv_obj_t* right_eye_closed_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;

    // Nose object
    lv_obj_t* nose_ = nullptr;

    // Whiskers
    lv_obj_t* whiskers_left_[3] = {nullptr};
    lv_obj_t* whiskers_right_[3] = {nullptr};

    // Mouth/teeth objects
    lv_obj_t* mouth_smile_ = nullptr;
    lv_obj_t* mouth_frown_ = nullptr;
    lv_obj_t* mouth_open_ = nullptr;
    lv_obj_t* mouth_line_ = nullptr;
    lv_obj_t* teeth_ = nullptr;

    // Layout constants
    static constexpr int kEyeY = 80;
    static constexpr int kEyeSpacing = 45;
    static constexpr int kEyeRadius = 16;
    static constexpr int kPupilRadius = 8;
    static constexpr int kNoseY = 125;
    static constexpr int kMouthY = 155;

    // Blink timing
    static constexpr int kBlinkInterval = 100;
    static constexpr int kBlinkDuration = 5;
};

#endif // FACE_RABBIT_H
