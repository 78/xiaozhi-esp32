#ifndef FACE_CAT_H
#define FACE_CAT_H

#include "vector_face.h"

/**
 * Cat face - feline design.
 * Features: Almond/slanted eyes, triangle nose, whiskers, curved smile
 */
class CatFace : public VectorFace {
public:
    CatFace();
    ~CatFace() override;

    void Create(lv_obj_t* parent) override;
    void Update() override;
    void Animate(int frame) override;

    const char* GetName() const override { return "Cat"; }
    const char* GetId() const override { return "cat"; }

private:
    void UpdateEyes();
    void UpdateMouth();

    // Eye objects (almond shaped - using ovals)
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_eye_closed_ = nullptr;
    lv_obj_t* right_eye_closed_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;

    // Nose object (triangle-ish)
    lv_obj_t* nose_ = nullptr;

    // Whiskers
    lv_obj_t* whiskers_left_[3] = {nullptr};
    lv_obj_t* whiskers_right_[3] = {nullptr};

    // Mouth objects
    lv_obj_t* mouth_smile_ = nullptr;
    lv_obj_t* mouth_frown_ = nullptr;
    lv_obj_t* mouth_open_ = nullptr;
    lv_obj_t* mouth_line_ = nullptr;

    // Layout constants
    static constexpr int kEyeY = 85;
    static constexpr int kEyeSpacing = 50;
    static constexpr int kEyeWidth = 28;
    static constexpr int kEyeHeight = 22;
    static constexpr int kNoseY = 130;
    static constexpr int kMouthY = 165;

    // Blink timing
    static constexpr int kBlinkInterval = 80;
    static constexpr int kBlinkDuration = 4;
};

#endif // FACE_CAT_H
