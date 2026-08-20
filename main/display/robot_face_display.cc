#include "robot_face_display.h"

#include "application.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>

#include "jpg/image_to_jpeg.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>

#define TAG "RobotFace"

// Bring-up aids: a framebuffer dump over serial and a whole-panel blink used to
// identify the board. Both are off in normal builds.
//
// NOTE: DumpSnapshot() renders the LVGL widget tree into a fresh buffer via
// lv_snapshot_take(). It does NOT read the panel, so a correct-looking capture
// proves the widgets are right and says nothing about whether pixels reached the
// display. Use IdentifyBlink(), which writes straight to the panel, for that.
#ifndef ROBOT_FACE_DEBUG_SNAPSHOT
#define ROBOT_FACE_DEBUG_SNAPSHOT 0
#endif
// Traces the speech envelope and the resulting mouth aperture while talking.
#ifndef ROBOT_FACE_DEBUG_MOUTH
#define ROBOT_FACE_DEBUG_MOUTH 0
#endif

namespace {

constexpr int kFrameMs = 33;      // ~30 fps
constexpr int kBlinkMs = 150;     // one full close-and-open
constexpr float kBlinkMinS = 2.2f;
constexpr float kBlinkMaxS = 6.5f;

// Eased toward the target at this fraction per frame. Separate rate for the
// mouth so speech tracks the audio envelope instead of lagging behind it.
constexpr float kEase = 0.28f;
constexpr float kMouthEase = 0.55f;

std::atomic<uint32_t> g_invalidate{0}, g_render_ready{0}, g_flush_start{0}, g_flush_finish{0};

inline float Approach(float current, float target, float k) {
    return current + (target - current) * k;
}

}  // namespace

// ---------------------------------------------------------------------------
// Expression table
// ---------------------------------------------------------------------------

const RobotFaceDisplay::FaceParams& RobotFaceDisplay::ParamsForEmotion(const std::string& emotion) {
    //                     eyeHL eyeHR eyeW  rad  gazeX gazeY browL browR browDy browOpa mouthW mouthOpen curve
    static const FaceParams kNeutral    = { 76, 76, 66, 22,   0,   0,    0,    0,    0,     0, 84,  0,  0.00f};
    static const std::map<std::string, FaceParams> kTable = {
        {"neutral",      kNeutral},
        {"happy",        { 58, 58, 70, 26,   0,   0,   -8,   -8,   -4,   140, 88,  6,  1.00f}},
        {"laughing",     { 34, 34, 74, 17,   0,  -2,  -10,  -10,   -6,   150, 92, 30,  1.00f}},
        {"funny",        { 66, 50, 72, 24,   6,   0,  -12,   -2,   -5,   160, 88, 14,  1.00f}},
        {"sad",          { 56, 56, 62, 20,   0,   6,  -14,  -14,    6,   200, 78,  0, -1.00f}},
        {"angry",        { 42, 42, 70, 14,   0,   0,   24,   24,    4,   255, 82,  4, -1.00f}},
        {"crying",       { 50, 50, 60, 20,   0,   8,  -12,  -12,    5,   200, 74, 12, -1.00f}},
        {"loving",       { 70, 70, 74, 30,   0,   0,   -6,   -6,   -4,   120, 88,  4,  1.00f}},
        {"embarrassed",  { 38, 38, 62, 16,  -8,   4,   -4,   -4,    2,   140, 76,  4,  0.35f}},
        {"surprised",    { 92, 92, 80, 34,   0,  -2,  -18,  -18,  -12,   220, 70, 26,  0.00f}},
        {"shocked",      {100,100, 86, 38,   0,  -4,  -22,  -22,  -16,   255, 66, 36,  0.00f}},
        {"thinking",     { 60, 44, 64, 20,  14,  -8,    8,   -6,   -2,   180, 72,  0,  0.00f}},
        {"winking",      {  8, 76, 68, 22,   0,   0,  -14,   -6,   -4,   150, 86,  6,  1.00f}},
        {"cool",         { 54, 54, 72, 18,   0,   0,   -4,   -4,   -2,   120, 86,  0,  0.60f}},
        {"relaxed",      { 48, 48, 68, 22,   0,   2,   -2,   -2,    0,   100, 84,  0,  0.70f}},
        {"delicious",    { 42, 42, 68, 20,   0,   0,   -8,   -8,   -4,   140, 84, 18,  1.00f}},
        {"kissy",        { 50, 50, 64, 22,   0,   0,   -6,   -6,   -3,   120, 40, 18,  0.00f}},
        {"confident",    { 56, 56, 70, 20,   0,   0,  -14,  -14,   -6,   180, 86,  0,  0.80f}},
        {"sleepy",       { 18, 18, 66, 9,    0,   6,    4,    4,    6,   120, 70,  8,  0.00f}},
        {"silly",        { 66, 66, 70, 24, -10,   4,  -14,  -14,   -6,   160, 88, 20,  1.00f}},
        {"confused",     { 64, 46, 66, 22,  10,   0,   10,   -8,   -2,   180, 78,  6, -0.40f}},
    };
    auto it = kTable.find(emotion);
    return it != kTable.end() ? it->second : kNeutral;
}

// ---------------------------------------------------------------------------

RobotFaceDisplay::RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io,
                                   esp_lcd_panel_handle_t panel, int width, int height,
                                   int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                                   bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {
    // Lay the face out around the centre of whatever space is left below the
    // status strip, rather than at fixed fractions of the panel height. That
    // keeps it composed on a tall portrait panel as well as a wide one.
    constexpr int kTopBarH = 36;
    const int avail = height_ - kTopBarH;
    const int cy = kTopBarH + avail / 2;

    // Centre the face's *drawn extent*, not its midpoint: the eyes are tall
    // blocks and the mouth is a thin line, so a naive midpoint leaves the whole
    // face riding high with dead space underneath. The face runs from roughly
    // 58px above the eye centres (brow top) to 22px below the mouth centre (a
    // fully open mouth), so bias the pair down by half that difference.
    constexpr int kAboveEyes = 58;
    constexpr int kBelowMouth = 22;
    const int bias = (kAboveEyes - kBelowMouth) / 2;
    const int gap = static_cast<int>(avail * 0.32f);  // eye centre -> mouth centre

    face_cx_ = width_ / 2;
    eyes_cy_ = cy + bias - gap / 2;
    mouth_cy_ = cy + bias + gap / 2;
    eye_gap_ = static_cast<int>(width_ * 0.21f);
    brow_len_ = static_cast<int>(width_ * 0.19f);
    mouth_arc_r_ = static_cast<int>(width_ * 0.16f);

    face_color_ = lv_color_hex(0x22D3EE);   // cyan, reads well on the ST7789
    shine_color_ = lv_color_hex(0xE6FBFF);

    current_ = ParamsForEmotion("neutral");
    target_ = current_;
    next_blink_us_ = esp_timer_get_time() + 1500000;
}

RobotFaceDisplay::~RobotFaceDisplay() {
    if (frame_timer_ != nullptr) {
        lv_timer_delete(frame_timer_);
        frame_timer_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void RobotFaceDisplay::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    Display::SetupUI();
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x060A10), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Full-screen backdrop the face is drawn onto. Created first so the status
    // bar built afterwards stacks above it.
    face_root_ = lv_obj_create(screen);
    lv_obj_set_size(face_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(face_root_, 0, 0);
    lv_obj_set_style_radius(face_root_, 0, 0);
    lv_obj_set_style_pad_all(face_root_, 0, 0);
    lv_obj_set_style_border_width(face_root_, 0, 0);
    lv_obj_set_style_bg_color(face_root_, lv_color_hex(0x060A10), 0);
    lv_obj_set_style_bg_opa(face_root_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(face_root_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(face_root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(face_root_, LV_LAYOUT_NONE, 0);

    // LcdDisplay treats container_ as the page background; point it at the
    // face backdrop so inherited code has a live object to work with.
    container_ = face_root_;

    BuildFace();
    BuildStatusBar();

    RenderFace();
    if (display_ != nullptr) {
        for (lv_event_code_t code : {LV_EVENT_INVALIDATE_AREA, LV_EVENT_RENDER_READY,
                                     LV_EVENT_FLUSH_START, LV_EVENT_FLUSH_FINISH}) {
            lv_display_add_event_cb(display_, DispEventCb, code, this);
        }
    }

    frame_timer_ = lv_timer_create(FrameCb, kFrameMs, this);

#if ROBOT_FACE_DEBUG_SNAPSHOT
    xTaskCreate(DebugSnapshotTask, "face_snap", 6144, this, 1, nullptr);
#endif
}

void RobotFaceDisplay::BuildFace() {
    auto mk_eye = [&](lv_obj_t** eye, lv_obj_t** shine) {
        *eye = lv_obj_create(face_root_);
        lv_obj_set_style_bg_color(*eye, face_color_, 0);
        lv_obj_set_style_bg_opa(*eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*eye, 0, 0);
        lv_obj_set_style_pad_all(*eye, 0, 0);
        lv_obj_remove_flag(*eye, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(*eye, LV_SCROLLBAR_MODE_OFF);

        // Cartoon catch-light in the upper-left of each eye.
        *shine = lv_obj_create(*eye);
        lv_obj_set_style_bg_color(*shine, shine_color_, 0);
        lv_obj_set_style_bg_opa(*shine, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*shine, 0, 0);
        lv_obj_set_style_pad_all(*shine, 0, 0);
        lv_obj_remove_flag(*shine, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(*shine, LV_SCROLLBAR_MODE_OFF);
    };
    mk_eye(&eye_l_, &shine_l_);
    mk_eye(&eye_r_, &shine_r_);

    auto mk_brow = [&](lv_obj_t** brow) {
        *brow = lv_line_create(face_root_);
        lv_obj_set_pos(*brow, 0, 0);
        lv_obj_set_size(*brow, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_line_color(*brow, face_color_, 0);
        lv_obj_set_style_line_width(*brow, 8, 0);
        lv_obj_set_style_line_rounded(*brow, true, 0);
        lv_obj_set_style_bg_opa(*brow, LV_OPA_TRANSP, 0);
    };
    mk_brow(&brow_l_);
    mk_brow(&brow_r_);

    // Mouth while talking: a pill whose height follows the speech envelope.
    mouth_pill_ = lv_obj_create(face_root_);
    lv_obj_set_style_bg_color(mouth_pill_, face_color_, 0);
    lv_obj_set_style_bg_opa(mouth_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_pill_, 0, 0);
    lv_obj_set_style_pad_all(mouth_pill_, 0, 0);
    lv_obj_remove_flag(mouth_pill_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(mouth_pill_, LV_SCROLLBAR_MODE_OFF);

    // Mouth while quiet: an arc, curving up to smile or down to frown.
    mouth_arc_ = lv_arc_create(face_root_);
    lv_obj_remove_style(mouth_arc_, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_width(mouth_arc_, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mouth_arc_, face_color_, LV_PART_MAIN);
    lv_obj_set_style_arc_width(mouth_arc_, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(mouth_arc_, true, LV_PART_MAIN);
    lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
}

void RobotFaceDisplay::BuildStatusBar() {
    auto screen = lv_screen_active();
    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    const lv_color_t fg = lv_color_hex(0x8FA3B0);  // dim, so it never fights the face

    top_bar_ = lv_obj_create(screen);
    lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_style_pad_top(top_bar_, 4, 0);
    lv_obj_set_style_pad_left(top_bar_, 8, 0);
    lv_obj_set_style_pad_right(top_bar_, 8, 0);
    lv_obj_set_style_text_font(top_bar_, text_font, 0);
    lv_obj_set_style_text_color(top_bar_, fg, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, fg, 0);

    status_label_ = lv_label_create(top_bar_);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_color(status_label_, fg, 0);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_flex_grow(status_label_, 1);

    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(right_icons, LV_OBJ_FLAG_SCROLLABLE);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, fg, 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, fg, 0);
    lv_obj_set_style_margin_left(battery_label_, 4, 0);

    // Notifications overlay the status text in the same strip.
    notification_label_ = lv_label_create(screen);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.8);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_hex(0xE6FBFF), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 8, 0);

    // Caption strip for system messages. The activation code arrives this way,
    // so it has to be readable even though ordinary chat text is suppressed.
    system_label_ = lv_label_create(screen);
    lv_obj_set_width(system_label_, LV_HOR_RES - 16);
    lv_label_set_long_mode(system_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(system_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(system_label_, text_font, 0);
    lv_obj_set_style_text_color(system_label_, lv_color_hex(0xE6FBFF), 0);
    lv_obj_set_style_bg_color(system_label_, lv_color_hex(0x0B141C), 0);
    lv_obj_set_style_bg_opa(system_label_, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(system_label_, 6, 0);
    lv_obj_set_style_radius(system_label_, 6, 0);
    lv_label_set_text(system_label_, "");
    lv_obj_align(system_label_, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);

    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

#if ROBOT_FACE_DEBUG_SNAPSHOT
void RobotFaceDisplay::DebugSnapshotTask(void* arg) {
    // Give the device time to finish activation and settle into idle.
    auto* self = static_cast<RobotFaceDisplay*>(arg);
    while (true) {
        if (self->want_snapshot_.exchange(false)) {
            self->DumpSnapshot();
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void RobotFaceDisplay::IdentifyBlink() {
    // Fill the whole panel straight through the LCD driver, bypassing LVGL, so
    // there is no doubt which physical board is on the end of the USB cable.
    if (panel_ == nullptr) {
        ESP_LOGE(TAG, "IDENTIFY: no panel handle");
        return;
    }
    const int strip_h = 20;
    auto* strip = static_cast<uint16_t*>(heap_caps_malloc(width_ * strip_h * 2, MALLOC_CAP_DMA));
    if (strip == nullptr) {
        ESP_LOGE(TAG, "IDENTIFY: no DMA memory");
        return;
    }
    ESP_LOGW(TAG, "IDENTIFY: blinking panel RED/WHITE 6 times - watch the board");
    // The LVGL port drives this same panel handle from its own task; take the
    // display lock so the two do not interleave SPI transactions and wedge it.
    DisplayLockGuard lock(this);
    const uint16_t colors[] = {0x00F8, 0xFFFF};  // red, white (panel byte order)
    for (int pass = 0; pass < 12; pass++) {
        uint16_t col = colors[pass % 2];
        for (int i = 0; i < width_ * strip_h; i++) strip[i] = col;
        for (int y = 0; y < height_; y += strip_h) {
            int h = (y + strip_h > height_) ? (height_ - y) : strip_h;
            esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + h, strip);
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    heap_caps_free(strip);
    lv_obj_invalidate(lv_screen_active());
    ESP_LOGW(TAG, "IDENTIFY: done - LVGL repaints the face next frame");
}

void RobotFaceDisplay::DumpSnapshot() {
    ESP_LOGI(TAG, "FACE geom screen=%dx%d cx=%d eyes_cy=%d gap=%d mouth_cy=%d", width_, height_,
             face_cx_, eyes_cy_, eye_gap_, mouth_cy_);
    auto report = [&](const char* name, lv_obj_t* o) {
        if (o == nullptr) {
            ESP_LOGW(TAG, "FACE %s is NULL", name);
            return;
        }
        ESP_LOGI(TAG, "FACE %-10s pos=(%d,%d) size=%dx%d hidden=%d", name, (int)lv_obj_get_x(o),
                 (int)lv_obj_get_y(o), (int)lv_obj_get_width(o), (int)lv_obj_get_height(o),
                 lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) ? 1 : 0);
    };
    report("face_root", face_root_);
    report("eye_l", eye_l_);
    report("eye_r", eye_r_);
    report("mouth_pill", mouth_pill_);
    report("top_bar", top_bar_);
    ESP_LOGI(TAG, "FACE frames rendered=%u  screen children=%d", (unsigned)frame_,
             (int)lv_obj_get_child_count(lv_screen_active()));
    ESP_LOGI(TAG, "FACE lvgl disp=%p default=%p panel=%p panel_io=%p", display_,
             lv_display_get_default(), panel_, panel_io_);
    ESP_LOGI(TAG, "FACE pipeline invalidate=%u render_ready=%u flush_start=%u flush_finish=%u",
             (unsigned)g_invalidate, (unsigned)g_render_ready, (unsigned)g_flush_start,
             (unsigned)g_flush_finish);

    // Deliberately NOT LvglDisplay::SnapshotToJpeg(): that byte-swaps the buffer
    // before encoding, which is right for panels whose framebuffer is already
    // swapped but wrong here -- this port swaps on flush instead, so the buffer
    // holds native RGB565 and swapping it again miscolours the capture.
    std::string jpg;
    {
        DisplayLockGuard lock(this);
        lv_draw_buf_t* buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
        if (buf == nullptr) {
            ESP_LOGE(TAG, "FACE snapshot failed");
            return;
        }
        uint16_t* px = reinterpret_cast<uint16_t*>(buf->data);
        ESP_LOGI(TAG, "FACE px bg=0x%04X eye=0x%04X (expect bg=0x0042 eye=0x269D)",
                 px[5 * buf->header.w + 5], px[eyes_cy_ * buf->header.w + face_cx_ - eye_gap_]);
        bool ok = image_to_jpeg_cb(
            buf->data, buf->data_size, buf->header.w, buf->header.h, V4L2_PIX_FMT_RGB565, 80,
            [](void* arg, size_t, const void* d, size_t len) -> size_t {
                if (d && len) static_cast<std::string*>(arg)->append(static_cast<const char*>(d), len);
                return len;
            },
            &jpg);
        lv_draw_buf_destroy(buf);
        if (!ok) {
            ESP_LOGE(TAG, "FACE jpeg encode failed");
            return;
        }
    }
    size_t cap = 4 * ((jpg.size() + 2) / 3) + 8;
    auto* b64 = static_cast<unsigned char*>(heap_caps_malloc(cap, MALLOC_CAP_SPIRAM));
    if (b64 == nullptr) {
        ESP_LOGE(TAG, "FACE no memory for base64");
        return;
    }
    size_t olen = 0;
    if (mbedtls_base64_encode(b64, cap, &olen, reinterpret_cast<const unsigned char*>(jpg.data()),
                              jpg.size()) == 0) {
        printf("\nSNAPSHOT_BEGIN %u\n", (unsigned)olen);
        for (size_t i = 0; i < olen; i += 512) {
            size_t n = olen - i < 512 ? olen - i : 512;
            printf("SNAP:%.*s\n", (int)n, b64 + i);
            vTaskDelay(pdMS_TO_TICKS(6));
        }
        printf("SNAPSHOT_END\n\n");
    }
    heap_caps_free(b64);
}
#endif  // ROBOT_FACE_DEBUG_SNAPSHOT

void RobotFaceDisplay::SetTheme(Theme* theme) {
    if (theme == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    auto* lvgl_theme = static_cast<LvglTheme*>(theme);

    // Only the status strip follows the theme, and only its fonts: its colour is
    // deliberately dim so it never competes with the face, and the face itself is
    // a fixed cyan-on-near-black palette by design.
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    if (status_label_ != nullptr) {
        lv_obj_set_style_text_font(status_label_, text_font, 0);
    }
    if (notification_label_ != nullptr) {
        lv_obj_set_style_text_font(notification_label_, text_font, 0);
    }
    // Must be rebound here too: LvglDisplay::SetTextFont() frees the previous
    // font as soon as this returns, so any label left pointing at it dangles
    // and faults the moment it lays out text.
    if (system_label_ != nullptr) {
        lv_obj_set_style_text_font(system_label_, text_font, 0);
    }
    for (lv_obj_t* icon : {network_label_, mute_label_, battery_label_}) {
        if (icon != nullptr) {
            lv_obj_set_style_text_font(icon, icon_font, 0);
        }
    }
    if (low_battery_label_ != nullptr) {
        lv_obj_set_style_text_font(low_battery_label_, text_font, 0);
    }
    if (low_battery_popup_ != nullptr) {
        lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    }

    Display::SetTheme(theme);
}

void RobotFaceDisplay::SetChatMessage(const char* role, const char* content) {
    // The face owns the screen, so assistant/user chat is deliberately dropped.
    // System messages are different: they carry the activation code, low-level
    // alerts and error text that the user has to be able to read.
    if (system_label_ == nullptr || role == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    if (std::strcmp(role, "system") != 0 || content == nullptr || content[0] == '\0') {
        lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(system_label_, content);
    lv_obj_remove_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
}

void RobotFaceDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    emotion_ = emotion;
    target_ = ParamsForEmotion(emotion_);
}

void RobotFaceDisplay::DispEventCb(lv_event_t* e) {
    switch (lv_event_get_code(e)) {
        case LV_EVENT_INVALIDATE_AREA: g_invalidate++; break;
        case LV_EVENT_RENDER_READY:    g_render_ready++; break;
        case LV_EVENT_FLUSH_START:     g_flush_start++; break;
        case LV_EVENT_FLUSH_FINISH:    g_flush_finish++; break;
        default: break;
    }
}

void RobotFaceDisplay::FrameCb(lv_timer_t* timer) {
    static_cast<RobotFaceDisplay*>(lv_timer_get_user_data(timer))->OnFrame();
}

void RobotFaceDisplay::OnFrame() {
    frame_++;
    const float t = frame_ * (kFrameMs / 1000.0f);
    const int64_t now = esp_timer_get_time();

    // Blink scheduling. Wide-eyed expressions hold their stare.
    const bool blink_ok = emotion_ != "surprised" && emotion_ != "shocked";
    if (!blinking_ && blink_ok && now >= next_blink_us_) {
        blinking_ = true;
        blink_start_us_ = now;
    }
    if (blinking_) {
        const float phase = (now - blink_start_us_) / (kBlinkMs * 1000.0f);
        if (phase >= 1.0f) {
            blinking_ = false;
            blink_ = 1.0f;
            const float gap = kBlinkMinS + (esp_random() % 1000) / 1000.0f * (kBlinkMaxS - kBlinkMinS);
            next_blink_us_ = now + static_cast<int64_t>(gap * 1000000.0f);
        } else {
            blink_ = 1.0f - std::sin(phase * static_cast<float>(M_PI));
        }
    }

    FaceParams want = target_;
    ApplyStateOverrides(want, t);

    current_.eye_h_l = Approach(current_.eye_h_l, want.eye_h_l, kEase);
    current_.eye_h_r = Approach(current_.eye_h_r, want.eye_h_r, kEase);
    current_.eye_w = Approach(current_.eye_w, want.eye_w, kEase);
    current_.eye_radius = Approach(current_.eye_radius, want.eye_radius, kEase);
    current_.gaze_x = Approach(current_.gaze_x, want.gaze_x, kEase);
    current_.gaze_y = Approach(current_.gaze_y, want.gaze_y, kEase);
    current_.brow_angle_l = Approach(current_.brow_angle_l, want.brow_angle_l, kEase);
    current_.brow_angle_r = Approach(current_.brow_angle_r, want.brow_angle_r, kEase);
    current_.brow_dy = Approach(current_.brow_dy, want.brow_dy, kEase);
    current_.brow_opa = Approach(current_.brow_opa, want.brow_opa, kEase);
    current_.mouth_w = Approach(current_.mouth_w, want.mouth_w, kEase);
    current_.mouth_open = Approach(current_.mouth_open, want.mouth_open, kMouthEase);
    current_.mouth_curve = Approach(current_.mouth_curve, want.mouth_curve, kEase);

#if ROBOT_FACE_DEBUG_MOUTH
    if (Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking) {
        printf("MOUTH,%u,%.3f,%.3f,%.1f\n", (unsigned)frame_,
               Application::GetInstance().GetAudioService().GetPlaybackLevel(), mouth_level_,
               current_.mouth_open);
        // Grab one frame with the mouth clearly open, as visual proof.
        if (!captured_open_mouth_ && current_.mouth_open > 26.0f) {
            captured_open_mouth_ = true;
            want_snapshot_.store(true);
        }
    }
#endif
    RenderFace();
}

void RobotFaceDisplay::ApplyStateOverrides(FaceParams& p, float t) {
    auto& app = Application::GetInstance();
    const DeviceState state = app.GetDeviceState();

    switch (state) {
        case kDeviceStateListening:
            // Lean in: eyes open wider, brows lift, mouth settles closed.
            p.eye_h_l *= 1.16f;
            p.eye_h_r *= 1.16f;
            p.eye_w *= 1.04f;
            p.brow_dy -= 7.0f;
            p.brow_opa = std::max(p.brow_opa, 110.0f);
            p.mouth_open = std::min(p.mouth_open, 5.0f);
            p.gaze_y -= 2.0f;
            break;

        case kDeviceStateSpeaking: {
            // The mouth is driven by the real amplitude going to the speaker.
            const float raw = app.GetAudioService().GetPlaybackLevel();
            const float k = raw > mouth_level_ ? 0.60f : 0.18f;  // fast attack, slow release
            mouth_level_ = mouth_level_ + (raw - mouth_level_) * k;
            p.mouth_open = 6.0f + mouth_level_ * 38.0f;
            p.mouth_curve *= 0.35f;  // an open mouth reads badly as a strong curve
            // A little life in the eyes while talking.
            p.gaze_x += std::sin(t * 1.7f) * 3.0f;
            p.eye_h_l *= 1.0f + mouth_level_ * 0.06f;
            p.eye_h_r *= 1.0f + mouth_level_ * 0.06f;
            break;
        }

        case kDeviceStateConnecting:
        case kDeviceStateStarting:
        case kDeviceStateActivating:
        case kDeviceStateUpgrading:
            // Scanning: narrowed eyes sweeping side to side.
            p.eye_h_l *= 0.55f;
            p.eye_h_r *= 0.55f;
            p.gaze_x = std::sin(t * 2.6f) * 16.0f;
            p.gaze_y = 0.0f;
            p.mouth_open = 0.0f;
            p.mouth_curve = 0.0f;
            p.brow_opa = 0.0f;
            break;

        case kDeviceStateFatalError:
            p.eye_h_l = 14.0f;
            p.eye_h_r = 14.0f;
            p.brow_angle_l = 26.0f;
            p.brow_angle_r = 26.0f;
            p.brow_opa = 255.0f;
            p.mouth_curve = -1.0f;
            p.mouth_open = 0.0f;
            break;

        default:
            // Idle: a slow drift and breath so it never looks frozen.
            mouth_level_ = 0.0f;
            p.gaze_x += std::sin(t * 0.63f) * 3.5f;
            p.gaze_y += std::sin(t * 0.41f) * 2.0f;
            break;
    }

    if (emotion_ == "thinking" && state != kDeviceStateSpeaking) {
        // Eyes flick between two upward corners, the way people search memory.
        p.gaze_x = std::sin(t * 1.1f) > 0.0f ? 15.0f : -13.0f;
        p.gaze_y = -8.0f;
    }
}

void RobotFaceDisplay::RenderFace() {
    const int gx = static_cast<int>(std::lround(current_.gaze_x));
    const int gy = static_cast<int>(std::lround(current_.gaze_y));
    const int ew = std::max(8, static_cast<int>(std::lround(current_.eye_w)));

    auto place_eye = [&](lv_obj_t* eye, lv_obj_t* shine, float h_param, int cx) {
        const int eh = std::max(4, static_cast<int>(std::lround(h_param * blink_)));
        const int r = std::min(static_cast<int>(std::lround(current_.eye_radius)),
                               std::min(ew, eh) / 2);
        lv_obj_set_size(eye, ew, eh);
        lv_obj_set_style_radius(eye, r, 0);
        lv_obj_set_pos(eye, cx - ew / 2 + gx, eyes_cy_ - eh / 2 + gy);

        // The catch-light only makes sense while the eye is actually open.
        if (eh > 26) {
            const int s = std::max(6, ew / 5);
            lv_obj_set_size(shine, s, s);
            lv_obj_set_style_radius(shine, s / 2, 0);
            lv_obj_set_pos(shine, ew / 6, eh / 6);
            lv_obj_remove_flag(shine, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shine, LV_OBJ_FLAG_HIDDEN);
        }
    };
    place_eye(eye_l_, shine_l_, current_.eye_h_l, face_cx_ - eye_gap_);
    place_eye(eye_r_, shine_r_, current_.eye_h_r, face_cx_ + eye_gap_);

    // Brows. Positive angle drops the inner end, which is what reads as angry.
    const lv_opa_t brow_opa =
        static_cast<lv_opa_t>(std::clamp(static_cast<int>(std::lround(current_.brow_opa)), 0, 255));
    auto place_brow = [&](lv_obj_t* brow, lv_point_precise_t* pts, float angle_deg, float eye_h,
                          int cx, bool inner_is_right) {
        if (brow_opa == 0) {
            lv_obj_add_flag(brow, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        const float a = angle_deg * static_cast<float>(M_PI) / 180.0f;
        const int hx = static_cast<int>(std::lround(brow_len_ / 2.0f * std::cos(a)));
        const int hy = static_cast<int>(std::lround(brow_len_ / 2.0f * std::sin(a)));
        const int bx = cx + gx;
        const int by = eyes_cy_ - static_cast<int>(eye_h * blink_) / 2 - 16 +
                       static_cast<int>(std::lround(current_.brow_dy)) + gy;
        // Left brow: inner end is the right-hand point, and vice versa.
        const int sign = inner_is_right ? 1 : -1;
        pts[0].x = bx - hx;
        pts[0].y = by - sign * hy;
        pts[1].x = bx + hx;
        pts[1].y = by + sign * hy;
        lv_line_set_points(brow, pts, 2);
        lv_obj_set_style_line_opa(brow, brow_opa, 0);
        lv_obj_remove_flag(brow, LV_OBJ_FLAG_HIDDEN);
    };
    place_brow(brow_l_, brow_pts_l_, current_.brow_angle_l, current_.eye_h_l,
               face_cx_ - eye_gap_, true);
    place_brow(brow_r_, brow_pts_r_, current_.brow_angle_r, current_.eye_h_r,
               face_cx_ + eye_gap_, false);

    // Mouth: pill while open or flat, arc while clearly smiling or frowning.
    const int mo = static_cast<int>(std::lround(current_.mouth_open));
    const float curve = current_.mouth_curve;
    const int mgx = static_cast<int>(std::lround(current_.gaze_x * 0.4f));
    const int mgy = static_cast<int>(std::lround(current_.gaze_y * 0.5f));

    if (mo < 10 && std::fabs(curve) > 0.18f) {
        const int R = mouth_arc_r_;
        lv_obj_set_size(mouth_arc_, 2 * R, 2 * R);
        if (curve > 0.0f) {
            // Bottom of the circle sits on the mouth line -> a smile.
            lv_arc_set_bg_angles(mouth_arc_, 35, 145);
            lv_obj_set_pos(mouth_arc_, face_cx_ - R + mgx, mouth_cy_ - 2 * R + mgy);
        } else {
            // Top of the circle sits on the mouth line -> a frown.
            lv_arc_set_bg_angles(mouth_arc_, 215, 325);
            lv_obj_set_pos(mouth_arc_, face_cx_ - R + mgx, mouth_cy_ + mgy);
        }
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_pill_, LV_OBJ_FLAG_HIDDEN);
    } else {
        const int mh = std::max(5, mo);
        const int mw = std::max(12, static_cast<int>(std::lround(current_.mouth_w)));
        lv_obj_set_size(mouth_pill_, mw, mh);
        lv_obj_set_style_radius(mouth_pill_, mh / 2, 0);
        lv_obj_set_pos(mouth_pill_, face_cx_ - mw / 2 + mgx, mouth_cy_ - mh / 2 + mgy);
        lv_obj_remove_flag(mouth_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    }
}
