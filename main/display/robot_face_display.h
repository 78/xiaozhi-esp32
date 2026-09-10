#ifndef ROBOT_FACE_DISPLAY_H
#define ROBOT_FACE_DISPLAY_H

#include "lcd_display.h"

#include <lvgl.h>

#include <atomic>
#include <string>

/*
 * A cartoon robot face drawn procedurally from plain LVGL widgets instead of
 * replayed from prebaked GIF sprites.
 *
 * Two rounded-rect eyes, a pair of straight brows and a mouth (a pill while
 * talking, an arc while smiling or frowning) are re-laid out every frame from
 * an eased set of geometry parameters.  Because nothing is prebaked the face
 * can react continuously: it blinks on its own schedule, widens when the
 * device starts listening, glances around while it thinks, and opens its mouth
 * in step with the actual amplitude of the speech coming out of the speaker.
 */
class RobotFaceDisplay : public SpiLcdDisplay {
public:
    RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                     int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                     bool swap_xy);
    ~RobotFaceDisplay();

    void SetupUI() override;

    // Debug aid: dumps the face geometry and a base64 JPEG of the panel to the
    // console, so the rendered screen can be inspected without a camera.
    void DumpSnapshot();
    void SetEmotion(const char* emotion) override;
    // The face keeps its own palette, so the stock theme pass (which repaints
    // the emoji widgets this display does not create) must not run.
    void SetTheme(Theme* theme) override;
    // Conversation text is dropped, but system messages are not - see the .cc.
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override {}

private:
    // One instant of face geometry. current_ is eased toward target_ each frame.
    struct FaceParams {
        float eye_h_l;      // left eye height, px
        float eye_h_r;      // right eye height, px
        float eye_w;        // eye width, px
        float eye_radius;   // corner radius, px
        float gaze_x;       // horizontal look offset, px
        float gaze_y;       // vertical look offset, px
        float brow_angle_l; // degrees, positive tilts the inner end down (angry)
        float brow_angle_r;
        float brow_dy;      // brow vertical offset, px (negative = raised)
        float brow_opa;     // 0..255, 0 hides the brows entirely
        float mouth_w;      // mouth width, px
        float mouth_open;   // aperture height, px
        float mouth_curve;  // -1 frown .. 0 flat .. +1 smile
    };

    static const FaceParams& ParamsForEmotion(const std::string& emotion);
    static FaceParams ScaledParams(const std::string& emotion);

    void BuildStatusBar();
    void BuildFace();
    static void FrameCb(lv_timer_t* timer);
    static void DebugSnapshotTask(void* arg);
    static void DispEventCb(lv_event_t* e);
    void IdentifyBlink();
    void OnFrame();
    void ApplyStateOverrides(FaceParams& p, float t);
    void RenderFace();

    // Layout anchors, derived from the panel size in the constructor.
    int face_cx_ = 0;
    int eyes_cy_ = 0;
    int eye_gap_ = 0;
    int brow_len_ = 0;
    int mouth_cy_ = 0;
    int mouth_h_ = 0;        // height of the mouth ellipse
    int mouth_bottom_ = 0;   // y of the mouth's lowest point

    lv_obj_t* face_root_ = nullptr;
    lv_obj_t* eye_l_ = nullptr;
    lv_obj_t* eye_r_ = nullptr;
    lv_obj_t* iris_l_ = nullptr;
    lv_obj_t* iris_r_ = nullptr;
    lv_obj_t* shine_l_ = nullptr;
    lv_obj_t* shine_r_ = nullptr;
    lv_obj_t* brow_l_ = nullptr;
    lv_obj_t* brow_r_ = nullptr;
    lv_obj_t* mouth_body_ = nullptr;   // filled mouth shape
    lv_obj_t* mouth_mask_ = nullptr;   // background-coloured cutter that shapes it
    lv_obj_t* system_label_ = nullptr;

    // lv_line does not copy its point array, so these must outlive the widget.
    lv_point_precise_t brow_pts_l_[2] = {};
    lv_point_precise_t brow_pts_r_[2] = {};

    lv_timer_t* frame_timer_ = nullptr;

    FaceParams current_{};
    FaceParams target_{};
    std::string emotion_ = "neutral";

    float blink_ = 1.0f;        // 1.0 open, 0.0 fully shut
    int64_t next_blink_us_ = 0; // when the next blink starts
    int64_t blink_start_us_ = 0;
    bool blinking_ = false;

    float mouth_level_ = 0.0f;  // smoothed speech envelope, 0..1
    std::atomic<bool> want_snapshot_{false};
    bool captured_open_mouth_ = false;
    uint32_t frame_ = 0;

    lv_color_t face_color_{};
    lv_color_t iris_color_{};
    lv_color_t shine_color_{};
};

#endif  // ROBOT_FACE_DISPLAY_H
