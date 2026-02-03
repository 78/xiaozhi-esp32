#ifndef VECTOR_FACE_H
#define VECTOR_FACE_H

#include <lvgl.h>

// Emotion types for vector faces
enum class FaceEmotion {
    kHappy,
    kSad,
    kThinking,
    kListening,
    kSpeaking,
    kSleepy,
    kExcited,
    kNeutral
};

// Eye state for animation
enum class EyeState {
    kOpen,
    kHalfOpen,
    kClosed,
    kSquint
};

// Mouth state for animation
enum class MouthState {
    kClosed,
    kSmile,
    kFrown,
    kOpen,
    kSpeaking
};

/**
 * Base class for vector-drawn animated faces.
 * Faces are drawn using LVGL widget objects on the parent container.
 * Display: 240x240 round GC9A01
 */
class VectorFace {
public:
    VectorFace();
    virtual ~VectorFace();

    /**
     * Create face UI elements on the given parent container.
     * Must be called before Draw().
     * @param parent LVGL parent object
     */
    virtual void Create(lv_obj_t* parent) = 0;

    /**
     * Destroy all face UI elements.
     */
    virtual void Destroy();

    /**
     * Update the face appearance based on current emotion.
     * Called after SetEmotion() or during animation.
     */
    virtual void Update() = 0;

    /**
     * Set the emotion to display.
     * @param emotion Emotion string (e.g., "happy", "sad", "thinking")
     */
    virtual void SetEmotion(const char* emotion);

    /**
     * Update animation frame (for blinking, breathing effects).
     * Called at ~30fps.
     * @param frame Current frame number (0-based, wraps around)
     */
    virtual void Animate(int frame) = 0;

    /**
     * Get the display name of this face.
     */
    virtual const char* GetName() const = 0;

    /**
     * Get the unique identifier for this face (used for NVS storage).
     */
    virtual const char* GetId() const = 0;

    /**
     * Check if face has been created.
     */
    bool IsCreated() const { return is_created_; }

    // Accessors
    FaceEmotion GetCurrentEmotion() const { return current_emotion_; }
    EyeState GetEyeState() const { return eye_state_; }
    MouthState GetMouthState() const { return mouth_state_; }

protected:
    // Convert emotion string to enum
    static FaceEmotion ParseEmotion(const char* emotion);

    // Helper to create a filled circle using LVGL object
    lv_obj_t* CreateCircle(lv_obj_t* parent, int x, int y, int radius, lv_color_t color);

    // Helper to create an oval using LVGL object
    lv_obj_t* CreateOval(lv_obj_t* parent, int x, int y, int width, int height, lv_color_t color);

    // Helper to create an arc using LVGL arc widget
    lv_obj_t* CreateArc(lv_obj_t* parent, int x, int y, int outer_radius, int inner_radius,
                        int start_angle, int end_angle, lv_color_t color);

    // Current state - default to happy/smiling
    FaceEmotion current_emotion_ = FaceEmotion::kHappy;
    EyeState eye_state_ = EyeState::kOpen;
    MouthState mouth_state_ = MouthState::kSmile;

    // Animation state
    int blink_counter_ = 0;
    int speak_counter_ = 0;
    bool is_created_ = false;

    // Parent container
    lv_obj_t* parent_ = nullptr;
    lv_obj_t* face_container_ = nullptr;

    // Display dimensions (centered on 240x240)
    static constexpr int kDisplayWidth = 240;
    static constexpr int kDisplayHeight = 240;
    static constexpr int kCenterX = 120;
    static constexpr int kCenterY = 120;
};

#endif // VECTOR_FACE_H
