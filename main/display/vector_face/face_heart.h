#ifndef FACE_HEART_H
#define FACE_HEART_H

#include "vector_face.h"

/**
 * Heart face - large smooth heart with gentle pulse animation.
 * Uses geometric shapes for smooth rendering.
 */
class HeartFace : public VectorFace {
public:
    HeartFace();
    ~HeartFace() override;

    void Create(lv_obj_t* parent) override;
    void Update() override;
    void Animate(int frame) override;

    const char* GetName() const override { return "Heart"; }
    const char* GetId() const override { return "heart"; }

private:
    // Heart shape parts (smooth circles and ovals)
    lv_obj_t* heart_left_ = nullptr;
    lv_obj_t* heart_right_ = nullptr;
    lv_obj_t* heart_center_ = nullptr;
    lv_obj_t* heart_bottom_ = nullptr;

    // Animation state
    float current_scale_ = 1.0f;
    int pulse_phase_ = 0;

    // Pulse timing (frames at 30fps) - SLOW gentle heartbeat
    static constexpr int kPulseCycle = 120;  // 4 seconds per heartbeat cycle
    static constexpr float kPulseMin = 0.97f;   // Minimal contraction
    static constexpr float kPulseMax = 1.03f;   // Subtle expansion
};

#endif // FACE_HEART_H
