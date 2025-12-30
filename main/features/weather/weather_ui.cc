#include "weather_ui.h"
#include "weather_icons.h"
#include "lunar_calendar.h"
#include <esp_log.h>
#include <time.h>
#include <cstdio>
#include <cmath>

#define TAG "WeatherUI"

// Colors from CSS
#define COLOR_PRIMARY lv_color_hex(0xd946ef)
#define COLOR_BG_MAIN lv_color_hex(0x000000) // Deep Black for Neon contrast
#define COLOR_TEXT_MAIN lv_color_hex(0xffffff)
#define COLOR_TEXT_SUB lv_color_hex(0xd1d5db)
#define COLOR_SUCCESS lv_color_hex(0x00ff99)
#define COLOR_DANGER lv_color_hex(0xff3366)

// Neon Colors
#define COLOR_NEON_CYAN lv_color_hex(0x00FFFF)
#define COLOR_NEON_MAGENTA lv_color_hex(0xFF00FF)
#define COLOR_NEON_GREEN lv_color_hex(0x39FF14)
#define COLOR_NEON_ORANGE lv_color_hex(0xFFA500)
#define COLOR_NEON_BLUE lv_color_hex(0x00BFFF)

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

// Uncomment the following line after generating the font file
#define USE_DIGITAL_FONT

#ifdef USE_DIGITAL_FONT
LV_FONT_DECLARE(font_digital_7_48);
#define IDLE_TIME_FONT &font_digital_7_48
#else
#define IDLE_TIME_FONT &lv_font_montserrat_48
#endif

LV_FONT_DECLARE(BUILTIN_ICON_FONT);

WeatherUI::WeatherUI() : idle_panel_(nullptr) {}

WeatherUI::~WeatherUI()
{
    if (idle_panel_ && lv_obj_is_valid(idle_panel_))
    {
        lv_obj_del(idle_panel_);
        idle_panel_ = nullptr;
    }
}

const char *WeatherUI::GetWeatherIcon(const std::string &code)
{
    if (code.size() < 2)
        return "\uf0c2"; // Cloud
    std::string prefix = code.substr(0, 2);
    if (prefix == "01")
        return "\uf185"; // Sun
    if (prefix == "02")
        return "\uf6c4"; // Cloud Sun
    if (prefix == "03" || prefix == "04")
        return "\uf0c2"; // Cloud
    if (prefix == "09" || prefix == "10")
        return "\uf740"; // Rain
    if (prefix == "11")
        return "\uf0e7"; // Bolt
    if (prefix == "13")
        return "\uf2dc"; // Snow
    if (prefix == "50")
        return "\uf72e"; // Wind
    return "\uf0c2";     // Cloud
}

static void StyleNeonBox(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x101010), 0); // Very dark grey background
    lv_obj_set_style_bg_opa(obj, LV_OPA_80, 0);                // Semi-transparent
    lv_obj_set_style_border_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 8, 0); // Rounded corners

    // Shadow for Glow Effect
    lv_obj_set_style_shadow_width(obj, 15, 0);
    lv_obj_set_style_shadow_color(obj, color, 0);
    lv_obj_set_style_shadow_spread(obj, 2, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_60, 0);
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
    lv_obj_set_style_outline_width(idle_panel_, 0, 0);
    lv_obj_set_style_pad_all(idle_panel_, 0, 0);
    lv_obj_set_scrollbar_mode(idle_panel_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);

    // --- Header (Top Bar) ---
    header_panel_ = lv_obj_create(idle_panel_);
    lv_obj_set_size(header_panel_, LV_PCT(100), 30);
    lv_obj_set_align(header_panel_, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(header_panel_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_panel_, 0, 0);
    lv_obj_set_flex_flow(header_panel_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_panel_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header_panel_, 10, 0);

    // Wifi Icon
    wifi_label_ = lv_label_create(header_panel_);
    lv_obj_set_style_text_font(wifi_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(wifi_label_, COLOR_NEON_GREEN, 0);
    lv_label_set_text(wifi_label_, "\uf1eb");

    // Title
    title_label_ = lv_label_create(header_panel_);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label_, COLOR_NEON_ORANGE, 0);
    lv_label_set_text(title_label_, "IoTForce AI Box");

    // Battery Icon
    battery_label_ = lv_label_create(header_panel_);
    lv_obj_set_style_text_font(battery_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(battery_label_, COLOR_NEON_GREEN, 0);
    lv_label_set_text(battery_label_, "\uf240");

    // --- Location (Above Time Box) ---
    lv_obj_t *loc_cont_top = lv_obj_create(idle_panel_);
    lv_obj_set_size(loc_cont_top, LV_PCT(100), 30);
    lv_obj_set_align(loc_cont_top, LV_ALIGN_TOP_MID);
    lv_obj_set_y(loc_cont_top, 35); // Below header (30px)
    lv_obj_set_style_bg_opa(loc_cont_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(loc_cont_top, 0, 0);
    lv_obj_set_flex_flow(loc_cont_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(loc_cont_top, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(loc_cont_top, 5, 0);

    location_icon_label_ = lv_label_create(loc_cont_top);
    lv_obj_set_style_text_font(location_icon_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(location_icon_label_, COLOR_NEON_BLUE, 0);
    lv_label_set_text(location_icon_label_, "\uf3c5");

    city_label_ = lv_label_create(loc_cont_top);
    lv_obj_set_style_text_font(city_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(city_label_, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(city_label_, "City");

    // --- Main Time Box (Center) ---
    lv_obj_t *time_box = lv_obj_create(idle_panel_);
    lv_obj_set_size(time_box, 200, 100);
    lv_obj_set_align(time_box, LV_ALIGN_CENTER);
    StyleNeonBox(time_box, COLOR_NEON_CYAN);
    lv_obj_set_flex_flow(time_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(time_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(time_box, LV_SCROLLBAR_MODE_OFF);

    time_label_ = lv_label_create(time_box);
    lv_obj_set_style_text_font(time_label_, IDLE_TIME_FONT, 0);
    lv_obj_set_style_text_color(time_label_, COLOR_NEON_CYAN, 0);
    lv_label_set_text(time_label_, "00:00");

    date_label_ = lv_label_create(time_box);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label_, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(date_label_, "Mon 01/01");

    // --- Bottom Info Grid ---
    lv_obj_t *grid_cont = lv_obj_create(idle_panel_);
    lv_obj_set_size(grid_cont, LV_PCT(100), 80);
    lv_obj_set_align(grid_cont, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_set_flex_flow(grid_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(grid_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(grid_cont, 10, 0);

    // Box 1: Weather
    lv_obj_t *weather_box = lv_obj_create(grid_cont);
    lv_obj_set_size(weather_box, 70, 60);
    StyleNeonBox(weather_box, COLOR_NEON_MAGENTA);
    lv_obj_set_flex_flow(weather_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weather_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(weather_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(weather_box, 2, 0);

    icon_label_ = lv_label_create(weather_box);
    lv_obj_set_style_text_font(icon_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(icon_label_, COLOR_NEON_MAGENTA, 0);
    lv_label_set_text(icon_label_, "\uf0c2");

    temp_label_ = lv_label_create(weather_box);
    lv_obj_set_style_text_font(temp_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp_label_, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(temp_label_, "-- C");

    // Box 2: Humidity (Middle)
    lv_obj_t *hum_box = lv_obj_create(grid_cont);
    lv_obj_set_size(hum_box, 70, 60);
    StyleNeonBox(hum_box, COLOR_NEON_BLUE);
    lv_obj_set_flex_flow(hum_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hum_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(hum_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(hum_box, 2, 0);

    humidity_icon_label_ = lv_label_create(hum_box);
    lv_obj_set_style_text_font(humidity_icon_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(humidity_icon_label_, COLOR_NEON_BLUE, 0);
    lv_label_set_text(humidity_icon_label_, "\uf0c2"); // Cloud icon (fallback for humidity)

    humidity_label_ = lv_label_create(hum_box);
    lv_obj_set_style_text_font(humidity_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(humidity_label_, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(humidity_label_, "-- %");

    // Box 3: UV/Air (Right)
    lv_obj_t *uv_box = lv_obj_create(grid_cont);
    lv_obj_set_size(uv_box, 70, 60);
    StyleNeonBox(uv_box, COLOR_NEON_ORANGE);
    lv_obj_set_flex_flow(uv_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(uv_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(uv_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(uv_box, 2, 0);

    uv_icon_label_ = lv_label_create(uv_box);
    lv_obj_set_style_text_font(uv_icon_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(uv_icon_label_, COLOR_NEON_ORANGE, 0);
    lv_label_set_text(uv_icon_label_, "\uf185");

    uv_label_ = lv_label_create(uv_box);
    lv_obj_set_style_text_font(uv_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(uv_label_, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(uv_label_, "UV");

    // Initialize unused pointers to null or create dummy objects if needed to prevent crashes
    // Since we changed layout, some pointers like pm25_label_ might be unused in this specific design
    // Let's add PM2.5 to the UV box or create a 4th box if space permits.
    // For now, let's just map pm25_label_ to a hidden object or reuse UV box for simplicity
    // Or better, let's add it to the UV box as a second line if needed, but for "Sporty" look, less is more.
    // We will just create it but hide it to avoid null pointer crashes in UpdateIdleDisplay
    pm25_label_ = lv_label_create(uv_box);
    lv_obj_add_flag(pm25_label_, LV_OBJ_FLAG_HIDDEN);
}

void WeatherUI::ShowIdleCard(const IdleCardInfo &info)
{
    if (!idle_panel_)
        return;

    lv_label_set_text(time_label_, info.time_text.c_str());

    // Combine Date and Lunar Date if needed, or just show Date
    // Image shows "T3 23/12/25"
    lv_label_set_text(date_label_, info.date_text.c_str());

    if (info.icon)
        lv_label_set_text(icon_label_, info.icon);

    lv_label_set_text(temp_label_, info.temperature_text.c_str());
    lv_label_set_text(city_label_, info.city.c_str());

    lv_label_set_text(humidity_label_, info.humidity_text.c_str());
    lv_label_set_text(uv_label_, info.uv_text.c_str());
    lv_label_set_text(pm25_label_, info.pm25_text.c_str());

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

    // Format: T<DayOfWeek> dd/mm/yy
    // tm_wday: 0=Sun, 1=Mon, ...
    int wday = tm_buf.tm_wday == 0 ? 8 : tm_buf.tm_wday + 1; // CN, T2, T3...
    if (wday == 8)
    {
        strftime(buf, sizeof(buf), "CN %d/%m/%y", &tm_buf);
    }
    else
    {
        char wday_str[16];
        snprintf(wday_str, sizeof(wday_str), "T%d", wday);
        char date_part[16];
        strftime(date_part, sizeof(date_part), "%d/%m/%y", &tm_buf);
        snprintf(buf, sizeof(buf), "%s %s", wday_str, date_part);
    }
    card.date_text = buf;

    // Lunar
    card.lunar_date_text = LunarCalendar::GetLunarDateString(tm_buf.tm_mday, tm_buf.tm_mon + 1, tm_buf.tm_year + 1900);
    card.can_chi_year = LunarCalendar::GetCanChiYear(tm_buf.tm_year + 1900);

    // Weather
    if (weather_info.valid)
    {
        card.city = weather_info.city;
        snprintf(buf, sizeof(buf), "%.1f C", weather_info.temp);
        card.temperature_text = buf;
        card.icon = GetWeatherIcon(weather_info.icon_code);

        // Set Humidity
        snprintf(buf, sizeof(buf), "%d %%", weather_info.humidity);
        card.humidity_text = buf;

        // Set UV Index
        snprintf(buf, sizeof(buf), "%.1f UV", weather_info.uv_index);
        card.uv_text = buf;

        // Set PM2.5
        snprintf(buf, sizeof(buf), "%.1f PM2.5", weather_info.pm2_5);
        card.pm25_text = buf;
    }
    else
    {
        card.city = "Updating...";
        card.temperature_text = "--";
        card.icon = "\uf0c2"; // Cloud
        card.humidity_text = "-- %";
        card.uv_text = "-- UV";
        card.pm25_text = "-- PM2.5";
    }

    ShowIdleCard(card);
}
