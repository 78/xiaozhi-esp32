#include "vector_face.h"
#include <cstring>
#include <esp_log.h>

#define TAG "VectorFace"

VectorFace::VectorFace() {
}

VectorFace::~VectorFace() {
    Destroy();
}

void VectorFace::Destroy() {
    if (face_container_ != nullptr && is_created_) {
        // Check if LVGL is ready and object is valid before deleting
        if (lv_obj_is_valid(face_container_)) {
            lv_obj_del(face_container_);
        }
        face_container_ = nullptr;
    }
    is_created_ = false;
    parent_ = nullptr;
}

FaceEmotion VectorFace::ParseEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return FaceEmotion::kNeutral;
    }

    if (strcmp(emotion, "happy") == 0) {
        return FaceEmotion::kHappy;
    } else if (strcmp(emotion, "sad") == 0) {
        return FaceEmotion::kSad;
    } else if (strcmp(emotion, "thinking") == 0) {
        return FaceEmotion::kThinking;
    } else if (strcmp(emotion, "listening") == 0) {
        return FaceEmotion::kListening;
    } else if (strcmp(emotion, "speaking") == 0) {
        return FaceEmotion::kSpeaking;
    } else if (strcmp(emotion, "sleepy") == 0) {
        return FaceEmotion::kSleepy;
    } else if (strcmp(emotion, "excited") == 0) {
        return FaceEmotion::kExcited;
    }
    return FaceEmotion::kNeutral;
}

void VectorFace::SetEmotion(const char* emotion) {
    current_emotion_ = ParseEmotion(emotion);

    // Update eye and mouth states based on emotion
    switch (current_emotion_) {
        case FaceEmotion::kHappy:
            eye_state_ = EyeState::kOpen;
            mouth_state_ = MouthState::kSmile;
            break;
        case FaceEmotion::kSad:
            eye_state_ = EyeState::kHalfOpen;
            mouth_state_ = MouthState::kFrown;
            break;
        case FaceEmotion::kThinking:
            eye_state_ = EyeState::kSquint;
            mouth_state_ = MouthState::kClosed;
            break;
        case FaceEmotion::kListening:
            eye_state_ = EyeState::kOpen;
            mouth_state_ = MouthState::kSmile;  // Attentive smile while listening
            break;
        case FaceEmotion::kSpeaking:
            eye_state_ = EyeState::kOpen;
            mouth_state_ = MouthState::kSpeaking;
            break;
        case FaceEmotion::kSleepy:
            eye_state_ = EyeState::kClosed;
            mouth_state_ = MouthState::kClosed;
            break;
        case FaceEmotion::kExcited:
            eye_state_ = EyeState::kOpen;
            mouth_state_ = MouthState::kOpen;
            break;
        case FaceEmotion::kNeutral:
        default:
            eye_state_ = EyeState::kOpen;
            mouth_state_ = MouthState::kSmile;  // Friendly smile by default
            break;
    }

    // Update visual if created
    if (is_created_) {
        Update();
    }
}

lv_obj_t* VectorFace::CreateCircle(lv_obj_t* parent, int x, int y, int radius, lv_color_t color) {
    lv_obj_t* circle = lv_obj_create(parent);
    lv_obj_set_size(circle, radius * 2, radius * 2);
    lv_obj_set_pos(circle, x - radius, y - radius);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, color, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_set_scrollbar_mode(circle, LV_SCROLLBAR_MODE_OFF);
    return circle;
}

lv_obj_t* VectorFace::CreateOval(lv_obj_t* parent, int x, int y, int width, int height, lv_color_t color) {
    lv_obj_t* oval = lv_obj_create(parent);
    lv_obj_set_size(oval, width, height);
    lv_obj_set_pos(oval, x - width / 2, y - height / 2);
    lv_obj_set_style_radius(oval, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(oval, color, 0);
    lv_obj_set_style_bg_opa(oval, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(oval, 0, 0);
    lv_obj_set_style_pad_all(oval, 0, 0);
    lv_obj_set_scrollbar_mode(oval, LV_SCROLLBAR_MODE_OFF);
    return oval;
}

lv_obj_t* VectorFace::CreateArc(lv_obj_t* parent, int x, int y, int outer_radius, int inner_radius,
                                 int start_angle, int end_angle, lv_color_t color) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, outer_radius * 2, outer_radius * 2);
    lv_obj_set_pos(arc, x - outer_radius, y - outer_radius);

    // Configure arc appearance
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_bg_angles(arc, start_angle, end_angle);
    lv_arc_set_angles(arc, start_angle, end_angle);

    // Style the arc indicator (the drawn part)
    int arc_width = outer_radius - inner_radius;
    lv_obj_set_style_arc_width(arc, arc_width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    // Hide the background arc
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);

    // Hide the knob
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);

    // Disable interaction
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    return arc;
}
