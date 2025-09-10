#include "lvgl_theme.h"

LvglTheme::LvglTheme(const std::string& name) : Theme(name) {
}

lv_color_t LvglTheme::ParseColor(const std::string& color) {
    if (color.find("#") == 0) {
        // Convert #112233 to lv_color_t
        uint8_t r = strtol(color.substr(1, 2).c_str(), nullptr, 16);
        uint8_t g = strtol(color.substr(3, 2).c_str(), nullptr, 16);
        uint8_t b = strtol(color.substr(5, 2).c_str(), nullptr, 16);
        return lv_color_make(r, g, b);
    }
    return lv_color_black();
}

LvglThemeManager::LvglThemeManager() {
}

LvglTheme* LvglThemeManager::GetTheme(const std::string& theme_name) {
    auto it = themes_.find(theme_name);
    if (it != themes_.end()) {
        return it->second;
    }
    return nullptr;
}

void LvglThemeManager::RegisterTheme(const std::string& theme_name, LvglTheme* theme) {
    themes_[theme_name] = theme;
}
