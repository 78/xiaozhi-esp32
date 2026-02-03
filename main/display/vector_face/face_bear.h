#ifndef FACE_BEAR_H
#define FACE_BEAR_H

#include "vector_face.h"

/**
 * Bear face - minimalist design.
 * Features: 2 circle eyes, oval nose, curved smile mouth
 */
class BearFace : public VectorFace {
public:
    BearFace();
    ~BearFace() override;

    void Create(lv_obj_t* parent) override;
    void Update() override;
    void Animate(int frame) override;

    const char* GetName() const override { return "Bear"; }
    const char* GetId() const override { return "bear"; }

private:
    void UpdateEyes();
    void UpdateMouth();

    // Eye objects
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_eye_closed_ = nullptr;
    lv_obj_t* right_eye_closed_ = nullptr;

    // Nose object
    lv_obj_t* nose_ = nullptr;

    // Mouth objects
    lv_obj_t* mouth_smile_ = nullptr;
    lv_obj_t* mouth_frown_ = nullptr;
    lv_obj_t* mouth_open_ = nullptr;
    lv_obj_t* mouth_line_ = nullptr;

    // Layout constants
    static constexpr int kEyeY = 85;
    static constexpr int kEyeSpacing = 50;
    static constexpr int kEyeRadius = 18;
    static constexpr int kNoseY = 130;
    static constexpr int kNoseWidth = 30;
    static constexpr int kNoseHeight = 22;
    static constexpr int kMouthY = 165;

    // Blink timing (frames at 30fps)
    static constexpr int kBlinkInterval = 90;  // ~3 seconds
    static constexpr int kBlinkDuration = 6;   // ~0.2 seconds
};

#endif // FACE_BEAR_H
