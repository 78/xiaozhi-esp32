#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "weather_service.h"
#include "almanac_service.h"

#include <lvgl.h>

#include <memory>
#include <string>

class LvglRawImage;

class DashboardUI {
public:
    explicit DashboardUI(lv_obj_t* parent);

    void Show();
    void Hide();
    bool IsVisible() const;

    void UpdateClock();
    void UpdateNetwork();
    void UpdateWeather(const WeatherSnapshot& snapshot);
    void UpdateAlmanac(const AlmanacSnapshot& snapshot);

private:
    void RenderAlmanac();
    lv_obj_t* container_ = nullptr;
    lv_obj_t* network_label_ = nullptr;
    lv_obj_t* holiday_badge_label_ = nullptr;
    lv_obj_t* holiday_notice_label_ = nullptr;
    lv_obj_t* city_label_ = nullptr;
    lv_obj_t* aqi_badge_ = nullptr;
    lv_obj_t* weather_icon_image_ = nullptr;
    // Decoded descriptor for the PNG currently shown in weather_icon_image_.
    LvglRawImage* weather_raw_image_ = nullptr;
    // Icon code of the PNG loaded into weather_icon_image_; used to skip
    // reloading (and re-decoding) when the condition has not changed.
    std::string loaded_icon_code_;
    lv_obj_t* aqi_line_label_ = nullptr;
    lv_obj_t* weather_text_badge_ = nullptr;
    lv_obj_t* hour_label_ = nullptr;
    lv_obj_t* colon_label_ = nullptr;
    lv_obj_t* minute_label_ = nullptr;
    lv_obj_t* second_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* weekday_label_ = nullptr;
    lv_obj_t* lunar_label_ = nullptr;
    lv_obj_t* solar_term_label_ = nullptr;
    lv_obj_t* yi_badge_ = nullptr;
    lv_obj_t* yi_label_ = nullptr;
    lv_obj_t* ji_badge_ = nullptr;
    lv_obj_t* ji_label_ = nullptr;
    AlmanacSnapshot almanac_snapshot_;
    lv_obj_t* temp_icon_ = nullptr;
    lv_obj_t* temp_bar_ = nullptr;
    lv_obj_t* temp_value_ = nullptr;
    lv_obj_t* humid_icon_ = nullptr;
    lv_obj_t* humid_bar_ = nullptr;
    lv_obj_t* humid_value_ = nullptr;
};

#endif  // DASHBOARD_UI_H
