#include "weather_ui.h"
#include "lunar_calendar.h"
#include <esp_log.h>
#include <time.h>
#include <cstdio>
#include <cmath>

#define TAG "WeatherUI"

// Colors from CSS
#define COLOR_PRIMARY lv_color_hex(0xd946ef)
#define COLOR_BG_MAIN lv_color_hex(0x090014)
#define COLOR_TEXT_MAIN lv_color_hex(0xffffff)
#define COLOR_TEXT_SUB lv_color_hex(0xd1d5db)
#define COLOR_SUCCESS lv_color_hex(0x00ff99)
#define COLOR_DANGER lv_color_hex(0xff3366)

// Font Awesome Icons (Assuming they are available in the project)
#define FA_CLOUD "\uf0c2"
#define FA_SUN "\uf185"
#define FA_CLOUD_RAIN "\uf73d"
#define FA_BOLT "\uf0e7"
#define FA_SNOWFLAKE "\uf2dc"
#define FA_SMOG "\uf75f"

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);
// LV_FONT_DECLARE(font_awesome_30_4); // Assuming this exists in project

WeatherUI::WeatherUI() : idle_panel_(nullptr) {}

WeatherUI::~WeatherUI()
{
    // If idle_panel_ is valid, delete it to clean up memory
    if (idle_panel_ && lv_obj_is_valid(idle_panel_))
    {
        lv_obj_del(idle_panel_);
        idle_panel_ = nullptr;
    }
}

const char *WeatherUI::GetWeatherIcon(const std::string &code)
{
    if (code.size() < 2)
        return FA_CLOUD;
    std::string prefix = code.substr(0, 2);
    if (prefix == "01")
        return FA_SUN;
    if (prefix == "02")
        return FA_CLOUD;
    if (prefix == "03")
        return FA_CLOUD;
    if (prefix == "04")
        return FA_CLOUD;
    if (prefix == "09")
        return FA_CLOUD_RAIN;
    if (prefix == "10")
        return FA_CLOUD_RAIN;
    if (prefix == "11")
        return FA_BOLT;
    if (prefix == "13")
        return FA_SNOWFLAKE;
    if (prefix == "50")
        return FA_SMOG;
    return FA_CLOUD;
}

void WeatherUI::SetupIdleUI(lv_obj_t *parent, int screen_width, int screen_height)
{
    screen_width_ = screen_width;
    screen_height_ = screen_height;

    if (idle_panel_)
        return;

    // Main Panel
    idle_panel_ = lv_obj_create(parent);
    lv_obj_set_size(idle_panel_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(idle_panel_, COLOR_BG_MAIN, 0);
    lv_obj_set_style_border_width(idle_panel_, 0, 0);
    lv_obj_set_flex_flow(idle_panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(idle_panel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(idle_panel_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);

    // Time
    time_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label_, COLOR_PRIMARY, 0);

    // Date (Solar)
    date_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label_, COLOR_TEXT_MAIN, 0);

    // Lunar Date
    lunar_date_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(lunar_date_label_, &lv_font_montserrat_14, 0); // Use smaller font or custom font supporting Vietnamese if available
    lv_obj_set_style_text_color(lunar_date_label_, COLOR_TEXT_SUB, 0);

    // Weather Row
    lv_obj_t *row = lv_obj_create(idle_panel_);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 15, 0);

    icon_label_ = lv_label_create(row);
    // lv_obj_set_style_text_font(icon_label_, &font_awesome_30_4, 0); // Uncomment if font exists
    lv_obj_set_style_text_color(icon_label_, COLOR_SUCCESS, 0);

    temp_label_ = lv_label_create(row);
    lv_obj_set_style_text_font(temp_label_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(temp_label_, COLOR_TEXT_MAIN, 0);

    // Details (Humidity, etc.)
    detail_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(detail_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detail_label_, COLOR_TEXT_SUB, 0);

    // Environment (UV, PM2.5)
    env_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(env_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(env_label_, COLOR_DANGER, 0);

    // City
    city_label_ = lv_label_create(idle_panel_);
    lv_obj_set_style_text_font(city_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(city_label_, COLOR_PRIMARY, 0);
    lv_obj_set_style_margin_top(city_label_, 10, 0);
}

void WeatherUI::ShowIdleCard(const IdleCardInfo &info)
{
    if (!idle_panel_)
        return;

    lv_label_set_text(time_label_, info.time_text.c_str());
    lv_label_set_text(date_label_, info.date_text.c_str());

    std::string lunar_text = info.lunar_date_text + "\n" + info.can_chi_year;
    lv_label_set_text(lunar_date_label_, lunar_text.c_str());

    if (info.icon)
        lv_label_set_text(icon_label_, info.icon);
    lv_label_set_text(temp_label_, info.temperature_text.c_str());

    std::string details = "Hum: " + info.humidity_text + " | " + info.description_text;
    lv_label_set_text(detail_label_, details.c_str());

    std::string env = "UV: " + info.uv_text + " | PM2.5: " + info.pm25_text;
    lv_label_set_text(env_label_, env.c_str());

    lv_label_set_text(city_label_, info.city.c_str());

    lv_obj_remove_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);
}

void WeatherUI::HideIdleCard()
{
    if (idle_panel_)
    {
        lv_obj_add_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void WeatherUI::UpdateIdleDisplay(const WeatherInfo &weather_info)
{
    IdleCardInfo card;

    // Time & Date
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M", &tm_buf);
    card.time_text = buf;

    strftime(buf, sizeof(buf), "%d/%m/%Y", &tm_buf);
    card.date_text = buf;

    // Lunar
    card.lunar_date_text = LunarCalendar::GetLunarDateString(tm_buf.tm_mday, tm_buf.tm_mon + 1, tm_buf.tm_year + 1900);
    card.can_chi_year = LunarCalendar::GetCanChiYear(tm_buf.tm_year + 1900); // Simplified

    // Weather
    if (weather_info.valid)
    {
        card.city = weather_info.city;
        snprintf(buf, sizeof(buf), "%.1f C", weather_info.temp);
        card.temperature_text = buf;
        card.humidity_text = std::to_string(weather_info.humidity) + "%";
        card.description_text = weather_info.description;
        card.icon = GetWeatherIcon(weather_info.icon_code);

        snprintf(buf, sizeof(buf), "%.1f", weather_info.uv_index);
        card.uv_text = buf;

        snprintf(buf, sizeof(buf), "%.1f", weather_info.pm2_5);
        card.pm25_text = buf;
    }
    else
    {
        card.city = "Updating...";
        card.temperature_text = "--";
        card.icon = FA_CLOUD;
    }

    ShowIdleCard(card);
}
