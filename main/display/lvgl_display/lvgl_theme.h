#pragma once

#include "display.h"
#include "lvgl_image.h"
#include "lvgl_font.h"
#include "emoji_collection.h"

#include <lvgl.h>
#include <memory>
#include <map>
#include <string>


class LvglTheme : public Theme {
public:
    static lv_color_t ParseColor(const std::string& color);

    LvglTheme(const std::string& name);

    // Properties
    inline lv_color_t background_color() const { return background_color_; }
    inline lv_color_t text_color() const { return text_color_; }
    inline lv_color_t chat_background_color() const { return chat_background_color_; }
    inline lv_color_t user_bubble_color() const { return user_bubble_color_; }
    inline lv_color_t assistant_bubble_color() const { return assistant_bubble_color_; }
    inline lv_color_t system_bubble_color() const { return system_bubble_color_; }
    inline lv_color_t system_text_color() const { return system_text_color_; }
    inline lv_color_t border_color() const { return border_color_; }
    inline lv_color_t low_battery_color() const { return low_battery_color_; }
    inline std::shared_ptr<LvglImage> background_image() const { return background_image_; }
    inline std::shared_ptr<EmojiCollection> emoji_collection() const { return emoji_collection_; }
    inline std::shared_ptr<LvglFont> text_font() const { return text_font_; }
    inline std::shared_ptr<LvglFont> icon_font() const { return icon_font_; }
    inline std::shared_ptr<LvglFont> large_icon_font() const { return large_icon_font_; }
    inline int spacing(int scale) const { return spacing_ * scale; }

    inline void set_background_color(lv_color_t background) { background_color_ = background; }
    inline void set_text_color(lv_color_t text) { text_color_ = text; }
    inline void set_chat_background_color(lv_color_t chat_background) { chat_background_color_ = chat_background; }
    inline void set_user_bubble_color(lv_color_t user_bubble) { user_bubble_color_ = user_bubble; }
    inline void set_assistant_bubble_color(lv_color_t assistant_bubble) { assistant_bubble_color_ = assistant_bubble; }
    inline void set_system_bubble_color(lv_color_t system_bubble) { system_bubble_color_ = system_bubble; }
    inline void set_system_text_color(lv_color_t system_text) { system_text_color_ = system_text; }
    inline void set_border_color(lv_color_t border) { border_color_ = border; }
    inline void set_low_battery_color(lv_color_t low_battery) { low_battery_color_ = low_battery; }
    inline void set_background_image(std::shared_ptr<LvglImage> background_image) { background_image_ = background_image; }
    inline void set_emoji_collection(std::shared_ptr<EmojiCollection> emoji_collection) { emoji_collection_ = emoji_collection; }
    inline void set_text_font(std::shared_ptr<LvglFont> text_font) { text_font_ = text_font; }
    inline void set_icon_font(std::shared_ptr<LvglFont> icon_font) { icon_font_ = icon_font; }
    inline void set_large_icon_font(std::shared_ptr<LvglFont> large_icon_font) { large_icon_font_ = large_icon_font; }

private:
    int spacing_ = 2;

    // Colors
    lv_color_t background_color_;
    lv_color_t text_color_;
    lv_color_t chat_background_color_;
    lv_color_t user_bubble_color_;
    lv_color_t assistant_bubble_color_;
    lv_color_t system_bubble_color_;
    lv_color_t system_text_color_;
    lv_color_t border_color_;
    lv_color_t low_battery_color_;

    // Background image
    std::shared_ptr<LvglImage> background_image_ = nullptr;

    // fonts
    std::shared_ptr<LvglFont> text_font_ = nullptr;
    std::shared_ptr<LvglFont> icon_font_ = nullptr;
    std::shared_ptr<LvglFont> large_icon_font_ = nullptr;

    // Emoji collection
    std::shared_ptr<EmojiCollection> emoji_collection_ = nullptr;
};


class LvglThemeManager {
public:
    static LvglThemeManager& GetInstance() {
        static LvglThemeManager instance;
        return instance;
    }

    void RegisterTheme(const std::string& theme_name, LvglTheme* theme);
    LvglTheme* GetTheme(const std::string& theme_name);

private:
    LvglThemeManager();
    void InitializeDefaultThemes();

    std::map<std::string, LvglTheme*> themes_;
};
