#pragma once

#include "custom_lcd_display.h"

#include <cstdint>
#include <memory>

// Full-screen animated eyes on a dark background.
// Reuses the classic LcdDisplay layout (status bar, notifications, system messages)
// but replaces the emoji with two animated eyes driven by emotion and device state.
class EyesDisplay : public CustomLcdDisplay {
public:
    using CustomLcdDisplay::CustomLcdDisplay;
    ~EyesDisplay();

    void SetupUI() override;
    bool SetEmotion(const char* emotion) override;
    void SetStatus(const char* status) override;
    void SetChatMessage(const char* role, const char* content) override;
    void SetTheme(Theme* theme) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    // Shape of both eyes. Covers are per-mille of the eye height so they scale with blinking.
    struct Expression {
        int32_t width;
        int32_t height;
        int32_t top_cover;     // per-mille covered by the top lid
        int32_t top_angle;     // lid rotation for the left eye, 0.1 degree; mirrored on the right
        int32_t bottom_cover;  // per-mille covered by the round bottom lid (happy "^" shape)
        int32_t offset_x;
        int32_t offset_y;
        uint32_t color;
        bool wink_right;
    };

    struct Eye {
        lv_obj_t* body = nullptr;
        lv_obj_t* top_lid = nullptr;
        lv_obj_t* bottom_lid = nullptr;
    };

    enum class Mode { kIdle, kListening, kSpeaking };

    static Expression ExpressionFor(const char* emotion);
    static Expression Lerp(const Expression& a, const Expression& b, int32_t t);

    void CreateEye(Eye& eye);
    void ApplyDarkSurface();
    void ApplyEyes();
    void TransitionTo(const Expression& target);
    void SetMode(Mode mode);
    void StartBlink();
    void StartSaccade();

    lv_obj_t* face_ = nullptr;
    Eye eyes_[2];
    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* saccade_timer_ = nullptr;

    Expression from_{};
    Expression to_{};
    Expression current_{};
    int32_t openness_ = 1000;  // per-mille, animated by blinking
    int32_t bounce_y_ = 0;     // animated while speaking
    int32_t gaze_x_ = 0;       // animated idle glances
    int32_t gaze_y_ = 0;
    int32_t gaze_from_x_ = 0;
    int32_t gaze_from_y_ = 0;
    int32_t gaze_to_x_ = 0;
    int32_t gaze_to_y_ = 0;
    Mode mode_ = Mode::kIdle;
};
