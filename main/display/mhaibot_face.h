#ifndef MHAIBOT_FACE_H
#define MHAIBOT_FACE_H

#include <lvgl.h>

#include <cstdint>

/**
 * Minimal reusable MhaiBot face: two neutral rounded eyes with randomized
 * blinking driven by a single LVGL timer (no FreeRTOS task, no blocking delay).
 */
class MhaiBotFace {
public:
    struct Config {
        int eye_width = 64;
        int eye_height = 84;
        int eye_radius = 24;
        int eye_gap = 48;
        int closed_height = 10;
        int vertical_offset = -8;
        uint32_t blink_min_ms = 2500;
        uint32_t blink_max_ms = 6000;
        uint32_t blink_close_min_ms = 100;
        uint32_t blink_close_max_ms = 180;
    };

    // Emotions the face can represent. V1.1 renders every value as the same
    // neutral blinking eyes; the enum exists so future versions can add
    // per-emotion visuals without changing call sites.
    enum class Emotion {
        kNeutral,
        kRobot2,
        kHappy,
        kThinking,
        kSpeaking,
        kListening,
        kRelaxed,
        kConfident,
    };

    MhaiBotFace(lv_obj_t* parent, lv_color_t eye_color);
    MhaiBotFace(lv_obj_t* parent, lv_color_t eye_color, const Config& config);
    ~MhaiBotFace();

    MhaiBotFace(const MhaiBotFace&) = delete;
    MhaiBotFace& operator=(const MhaiBotFace&) = delete;

    void Show();
    void Hide();
    void SetColor(lv_color_t eye_color);
    void SetEmotion(Emotion emotion);

    bool IsVisible() const { return visible_; }
    Emotion GetEmotion() const { return emotion_; }

private:
    enum class BlinkPhase { Open, Closed };

    void CreateEyes(lv_obj_t* parent, lv_color_t eye_color);
    void StartBlinkTimer();
    void StopBlinkTimer();
    void OpenEyes();
    void CloseEyes();
    uint32_t RandomIntervalMs(uint32_t min_ms, uint32_t max_ms) const;
    static void BlinkTimerCb(lv_timer_t* timer);
    static void OnRootDeleted(lv_event_t* event);

    Config config_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_timer_t* blink_timer_ = nullptr;
    BlinkPhase phase_ = BlinkPhase::Open;
    Emotion emotion_ = Emotion::kNeutral;
    bool visible_ = false;
    bool destroying_ = false;
};

#endif  // MHAIBOT_FACE_H
