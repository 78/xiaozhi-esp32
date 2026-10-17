// R2D2 mini-jarvis screen: the R2D2 HUD's look on the 240×240 round panel.
//
// Idle: dark background, Wi-Fi icon, clock, date and the weather the R2D2
// Core reports (GET /satellite/status, see r2d2_status.h). In a conversation:
// the HUD's Jarvis face (r2d2_face.h) with the state label under it.
//
// The stock UI (LcdDisplay) is still built underneath and keeps working for
// everything this layer doesn't cover: while the device is starting,
// configuring Wi-Fi, activating or upgrading, the layer hides and the stock
// screen with its instructions shows through.
#pragma once

#include "display/lcd_display.h"
#include "r2d2_face.h"

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

    // From the status poller (any task).
    void SetWeather(const Weather& weather);

private:
    void Refresh();       // display lock held
    void UpdateClock();   // display lock held
    void ShowWeather();   // display lock held
    static void OnTick(lv_timer_t* timer);

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
    lv_timer_t* timer_ = nullptr;

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
