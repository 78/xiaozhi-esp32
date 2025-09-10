#include "lvgl_theme.h"

LvglTheme::LvglTheme(const std::string& name) : Theme(name) {
}

LV_FONT_DECLARE(LVGL_TEXT_FONT);
LV_FONT_DECLARE(LVGL_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);
LV_FONT_DECLARE(font_awesome_20_4);


LvglThemeManager::LvglThemeManager() : light_theme_("light"), dark_theme_("dark") {
    InitializeDefaultThemes();
}

LvglTheme* LvglThemeManager::GetTheme(const std::string& theme_name) {
    if (theme_name == "light") {
        return &light_theme_;
    } else if (theme_name == "dark") {
        return &dark_theme_;
    }
    return nullptr;
}

void LvglThemeManager::InitializeDefaultThemes() {
    auto text_font = std::make_shared<LvglBuiltInFont>(&LVGL_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&LVGL_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_4);

    // light theme
    light_theme_.set_background_color(lv_color_white());
    light_theme_.set_text_color(lv_color_black());
    light_theme_.set_chat_background_color(lv_color_hex(0xE0E0E0));
    light_theme_.set_user_bubble_color(lv_color_hex(0x95EC69));
    light_theme_.set_assistant_bubble_color(lv_color_white());
    light_theme_.set_system_bubble_color(lv_color_hex(0xE0E0E0));
    light_theme_.set_system_text_color(lv_color_hex(0x666666));
    light_theme_.set_border_color(lv_color_hex(0xE0E0E0));
    light_theme_.set_low_battery_color(lv_color_black());
    light_theme_.set_text_font(text_font);
    light_theme_.set_icon_font(icon_font);
    light_theme_.set_large_icon_font(large_icon_font);

    // dark theme
    dark_theme_.set_background_color(lv_color_hex(0x121212));
    dark_theme_.set_text_color(lv_color_white());
    dark_theme_.set_chat_background_color(lv_color_hex(0x1E1E1E));
    dark_theme_.set_user_bubble_color(lv_color_hex(0x1A6C37));
    dark_theme_.set_assistant_bubble_color(lv_color_hex(0x333333));
    dark_theme_.set_system_bubble_color(lv_color_hex(0x2A2A2A));
    dark_theme_.set_system_text_color(lv_color_hex(0xAAAAAA));
    dark_theme_.set_border_color(lv_color_hex(0x333333));
    dark_theme_.set_low_battery_color(lv_color_hex(0xFF0000));
    dark_theme_.set_text_font(text_font);
    dark_theme_.set_icon_font(icon_font);
    dark_theme_.set_large_icon_font(large_icon_font);
}