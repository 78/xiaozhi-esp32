#include "lvgl_theme.h"

LvglTheme::LvglTheme(const std::string& name) : Theme(name) {
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
