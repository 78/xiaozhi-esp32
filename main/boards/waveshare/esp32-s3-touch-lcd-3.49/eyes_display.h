#pragma once

#include "custom_lcd_display.h"

#include <cstdint>
#include <memory>
#include <string>

// Full-screen animated eyes ("robot visor") on a dark background.
//
// Expression priority, highest first:
//   runner game > asleep > temporary reaction (touch, startle, activity) > bored > server emotion
//
// Local triggers (independent of the server):
//   - touch: tap = surprised then happy, repeated taps = annoyed then angry,
//     long press = petted (loving), drag = eyes follow the finger
//   - idle: bored, random activities (jump, chase, read, sneeze, runner game), then asleep
//   - wake word / touch while asleep or playing = startled
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
    enum class Motion : uint8_t { kNone, kShake, kTremble, kBounce, kPulse, kScan, kSway, kBreath };
    enum class Mode : uint8_t { kIdle, kListening, kSpeaking };
    enum class Activity : uint8_t { kNone, kJump, kChase, kRead, kSneeze, kGame };

    // Shape of both eyes. Covers and right_height are per-mille so they scale with blinking.
    struct Expression {
        int32_t width = 120;
        int32_t height = 100;
        int32_t top_cover = 0;     // per-mille covered by the top lid
        int32_t top_angle = 0;     // lid rotation for the left eye, 0.1 degree; mirrored on the right
        int32_t bottom_cover = 0;  // per-mille covered by the round bottom lid ("^" happy shape)
        int32_t offset_x = 0;
        int32_t offset_y = 0;
        uint32_t color = 0x29D8FF;
        bool wink_right = false;
        int32_t right_height = 1000;  // right eye height relative to the left one
        int32_t inward = 0;           // px both eyes move toward the center (cross-eyed)
        Motion motion = Motion::kNone;
        bool tears = false;
        bool blush = false;
        bool question = false;
        bool zzz = false;
    };

    struct Eye {
        lv_obj_t* body = nullptr;
        lv_obj_t* top_lid = nullptr;
        lv_obj_t* bottom_lid = nullptr;
    };

    struct Obstacle {
        lv_obj_t* obj = nullptr;
        bool active = false;
        bool scored = false;
        bool will_miss = false;
        int32_t x = 0;
        int32_t w = 0;
        int32_t h = 0;
    };

    static constexpr int kObstacleCount = 3;

    static Expression ExpressionFor(const char* name);
    static Expression Lerp(const Expression& a, const Expression& b, int32_t t);

    // Construction
    void CreateEye(Eye& eye);
    void CreateDecor();
    void CreateGame();
    void ApplyDarkSurface();

    // Rendering
    void ApplyEyes();
    void TransitionTo(const char* name);
    void RefreshExpression(uint32_t now);
    const char* ResolveExpressionName(uint32_t now);

    // Ambient animations
    void StartBlink();
    void StartSaccade();
    void Tick();
    void UpdateMotion(uint32_t now);
    void UpdateIdle(uint32_t now);

    // Reactions and activities
    void NoteInteraction(uint32_t now, bool startle);
    void SetReaction(const char* name, uint32_t duration_ms, uint32_t now);
    void QueueReaction(const char* name, uint32_t duration_ms);
    void OnTouchEvent(lv_event_t* e);
    void StartActivity(Activity activity, uint32_t now);
    void StopActivity(uint32_t now);
    void UpdateActivity(uint32_t now);

    // Runner game
    void StartGame(uint32_t now);
    void UpdateGame(uint32_t now);

    // Widgets
    lv_obj_t* face_ = nullptr;
    Eye eyes_[2];
    lv_obj_t* blush_[2] = {nullptr, nullptr};
    lv_obj_t* tears_[2] = {nullptr, nullptr};
    lv_obj_t* question_ = nullptr;
    lv_obj_t* zzz_[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* dot_ = nullptr;
    lv_obj_t* game_ = nullptr;
    lv_obj_t* runner_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    Obstacle obstacles_[kObstacleCount];

    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* saccade_timer_ = nullptr;
    lv_timer_t* tick_timer_ = nullptr;

    // Expression state
    Expression from_{};
    Expression to_{};
    Expression current_{};
    std::string target_name_;
    std::string server_emotion_ = "neutral";
    const char* reaction_ = nullptr;
    uint32_t reaction_until_ = 0;
    const char* queued_reaction_ = nullptr;
    uint32_t queued_duration_ = 0;

    // Offsets added on top of the expression
    int32_t openness_ = 1000;  // per-mille, animated by blinking
    int32_t gaze_x_ = 0;       // idle glances (lv_anim)
    int32_t gaze_y_ = 0;
    int32_t gaze_from_x_ = 0;
    int32_t gaze_from_y_ = 0;
    int32_t gaze_to_x_ = 0;
    int32_t gaze_to_y_ = 0;
    int32_t motion_x_ = 0;  // per-expression motion and speaking bounce
    int32_t motion_y_ = 0;
    int32_t motion_scale_ = 1000;
    int32_t activity_x_ = 0;
    int32_t activity_y_ = 0;
    int32_t touch_x_ = 0;  // eyes follow the finger
    int32_t touch_y_ = 0;

    // Behaviour state
    Mode mode_ = Mode::kIdle;
    Activity activity_ = Activity::kNone;
    uint32_t activity_start_ = 0;
    uint32_t last_interaction_ = 0;
    uint32_t next_activity_ = 0;
    bool asleep_ = false;
    bool bored_ = false;
    int32_t annoyance_ = 0;
    uint32_t last_tap_ = 0;
    bool pressing_ = false;
    bool petting_ = false;

    // Runner game state
    int32_t runner_h10_ = 0;  // height above the ground, x10 fixed point
    int32_t runner_v10_ = 0;
    int32_t game_speed_ = 0;
    int32_t game_score_ = 0;
    int32_t next_gap_ = 0;
};
