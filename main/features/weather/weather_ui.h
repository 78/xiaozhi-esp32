#ifndef WEATHER_UI_H
#define WEATHER_UI_H

#include "weather_model.h"
#include <lvgl.h>

class WeatherUI
{
public:
    WeatherUI();
    ~WeatherUI();

    void SetupIdleUI(lv_obj_t *parent, int screen_width, int screen_height);
    void ShowIdleCard(const IdleCardInfo &info);
    void HideIdleCard();
    void UpdateIdleDisplay(const WeatherInfo &weather_info);

    static const char *GetWeatherIcon(const std::string &code);

private:
    lv_obj_t *idle_panel_;

    // UI Components
    lv_obj_t *time_label_;
    lv_obj_t *date_label_;
    lv_obj_t *lunar_date_label_;
    lv_obj_t *temp_label_;
    lv_obj_t *icon_label_;
    lv_obj_t *city_label_;
    lv_obj_t *detail_label_; // Humidity, Wind, etc.
    lv_obj_t *env_label_;    // UV, PM2.5

    int screen_width_;
    int screen_height_;
};

#endif // WEATHER_UI_H
