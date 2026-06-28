#ifndef ANIMATED_EYES_H
#define ANIMATED_EYES_H

#include <lvgl.h>
#include <cstdint>
#include <cstring>

// Eye geometry for a single eye
struct EyeState {
    int16_t cx;         // Center X
    int16_t cy;         // Center Y
    int16_t width;      // Half-width of eye opening
    int16_t height;     // Half-height of eye opening
    int16_t pupil_dx;   // Pupil offset from center X
    int16_t pupil_dy;   // Pupil offset from center Y
    int16_t pupil_r;    // Pupil radius
    int16_t lid_top;    // Top lid closure (0=open, height=closed)
    int16_t lid_bot;    // Bottom lid closure (0=open, height=closed)
    int16_t brow_angle; // Brow angle (-10 to 10, negative=angry)
    bool visible;       // Whether eye is drawn at all
};

// Mouth geometry
struct MouthState {
    int16_t cx;          // Center X
    int16_t cy;          // Center Y
    int16_t width;       // Half-width of mouth
    int16_t height;      // Half-height (0=flat line, positive=smile, negative=frown)
    int16_t open;        // Mouth opening (0=closed, >0=open amount in pixels)
    bool visible;
};

// Complete face with both eyes and mouth
struct FaceState {
    EyeState right_eye;
    EyeState left_eye;
    bool left_eye_open;
    MouthState mouth;
};

// Emotion preset indices
enum EmotionPreset {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_ANGRY,
    EMOTION_SURPRISED,
    EMOTION_DISGUSTED,
    EMOTION_FEARFUL,
    EMOTION_SLEEPY,
    EMOTION_CONFUSED,
    EMOTION_THINKING,
    EMOTION_WINK,
    EMOTION_COOL,
    EMOTION_LAUGHING,
    EMOTION_SHY,
    EMOTION_CRYING,
    EMOTION_LOVE,
    EMOTION_EXCITED,
    EMOTION_BORED,
    EMOTION_SMIRK,
    EMOTION_DETERMINED,
    EMOTION_RELAXED,
    EMOTION_COUNT
};

// Map emotion string to preset index
EmotionPreset emotion_string_to_preset(const char* emotion);

// Get the face state for a given emotion
const FaceState& get_emotion_face(EmotionPreset preset);

// Interpolate between two face states (t=0..256, 256=fully target)
void face_lerp(FaceState* out, const FaceState* from, const FaceState* to, int t);

// Draw a complete face onto an LVGL canvas buffer
// buf points to pixel data (after palette), stride is bytes per row
void draw_face(uint8_t* buf, int canvas_w, int canvas_h, int stride, const FaceState* face);

#endif // ANIMATED_EYES_H
