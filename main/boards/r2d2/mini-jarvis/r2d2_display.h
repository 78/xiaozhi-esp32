// R2D2 mini-jarvis screen: the R2D2 HUD's look on the 240×240 round panel.
//
// Idle: dark background, Wi-Fi icon, clock, date and the weather the R2D2
// Core reports (GET /satellite/status, see r2d2_core_client.h); swiping away
// from it opens the touch navigation (r2d2_nav.h). In a conversation:
// the HUD's Jarvis face (r2d2_face.h) with the state label under it. A chart
// the Core sends with an answer ({"r2d2": "chart"} custom message) takes the
// screen for its ttl, or until shortly after the conversation ends.
//
// Between night_start and night_end (local), an idle screen dims to the night
// level 10 s after the last touch or conversation, and lights up on either.
//
// The stock UI (LcdDisplay) is still built underneath and keeps working for
// everything this layer doesn't cover: while the device is starting,
// configuring Wi-Fi, activating or upgrading, the layer hides and the stock
// screen with its instructions shows through.
#pragma once

#include "display/lcd_display.h"

#include <cJSON.h>
#include "r2d2_face.h"
#include "r2d2_nav.h"

#include <esp_timer.h>

#include <atomic>
#include <memory>
#include <string>

namespace r2d2 {

struct Weather {
    bool up = false;
    float temp_c = 0;
    std::string condition;  // HUD label: "Chuva provável", "Estável"...
    std::string icon;       // clear / rain / storm / fog
    int rain_pct = -1;      // -1: unknown
};

class R2d2Display : public SpiLcdDisplay {
public:
    R2d2Display(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle, int width,
                int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);
    ~R2d2Display();

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void UpdateStatusBar(bool update_all = false) override;

    // From the Core client's task (any task): each takes the display lock.
    void AttachCore(CoreClient* client) { client_ = client; }
    void SetWeather(const Weather& weather);
    bool OnNav(cJSON* nav);  // takes ownership; false: the UI isn't up yet
    void OnScreen(uint32_t seq, const std::string& id, cJSON* screen);
    void OnImage(uint32_t seq, uint8_t* rgb565, size_t size, size_t w, size_t h, size_t stride);
    void OnAction(uint32_t seq, bool ok);
    // From the touch panel, via the main task.
    void OnGesture(Gesture gesture, int x, int y);
    // Night dimming while idle (window and level from the Core's status poll).
    void SetNight(int start_hour, int end_hour, int brightness);
    void NoteActivity() { last_activity_us_ = esp_timer_get_time(); }  // any task: a touch began

private:
    void Refresh();       // display lock held
    void UpdateClock();   // display lock held
    void ShowWeather();   // display lock held
    static void OnTick(lv_timer_t* timer);
    void ShowChart(const cJSON* payload);  // display lock held
    void UpdateNight(int64_t now_us);       // display lock held

    lv_obj_t* root_ = nullptr;
    lv_obj_t* idle_ = nullptr;
    lv_obj_t* talk_ = nullptr;
    lv_obj_t* wifi_label_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* condition_label_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    std::unique_ptr<Face> face_;
    std::unique_ptr<WeatherIcon> weather_icon_;
    lv_obj_t* chart_view_ = nullptr;
    lv_obj_t* chart_ = nullptr;
    lv_chart_series_t* chart_series_ = nullptr;
    lv_obj_t* chart_title_ = nullptr;
    lv_obj_t* chart_value_ = nullptr;
    lv_obj_t* chart_range_ = nullptr;
    lv_obj_t* chart_state_ = nullptr;
    int64_t chart_until_us_ = 0;
    int night_start_ = 20, night_end_ = 7, night_level_ = 10;
    bool night_dimmed_ = false;
    std::atomic<int64_t> last_activity_us_{0};
    int64_t next_night_check_us_ = 0;
    lv_timer_t* timer_ = nullptr;
    CoreClient* client_ = nullptr;
    std::unique_ptr<NavView> nav_;

    Weather weather_;
    bool weather_dirty_ = false;
    std::string emotion_;          // last emotion from the app or the Core
    std::string face_state_;       // what the face shows now ("" = idle screen)
    std::string face_hint_;
    std::string face_variant_;
    int64_t error_until_us_ = 0;   // an error/busy face held after an alert
    std::string error_variant_;
    int64_t next_variant_us_ = 0;
    int last_clock_min_ = -1;
    const char* wifi_icon_ = nullptr;
};

}  // namespace r2d2
