#ifndef OLED_HOME_SCREEN_H
#define OLED_HOME_SCREEN_H

#include <lvgl.h>
#include <esp_timer.h>
#include <functional>

class OledHomeScreen {
private:
    lv_obj_t* screen_ = nullptr;

    // 状态栏组件 (顶部)
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* wifi_icon_ = nullptr;
    lv_obj_t* status_label_ = nullptr;     // 中间状态文字 "待命"
    lv_obj_t* volume_icon_ = nullptr;
    lv_obj_t* volume_text_ = nullptr;      // 音量数值 "80" - 按键时显示
    esp_timer_handle_t volume_text_timer_ = nullptr;  // 音量文字自动隐藏定时器

    // 主时间区组件 (中间)
    lv_obj_t* main_time_area_ = nullptr;
    lv_obj_t* large_time_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;

    int width_, height_;
    bool is_128x64_;
    bool visible_ = false;

    const lv_font_t* small_font_;
    const lv_font_t* icon_font_;
    const lv_font_t* large_time_font_;     // 大时间字体

    esp_timer_handle_t update_timer_ = nullptr;

    // 用于切换默认界面
    std::function<void(bool)> on_visibility_changed_;

    void CreateStatusBar();
    void CreateMainTimeArea();
    void UpdateTime();
    void UpdateDate();
    void SetVisible(bool visible);
    void ShowVolumeText(int level);
    void HideVolumeText();

public:
    OledHomeScreen(int width, int height,
                   const lv_font_t* small_font,
                   const lv_font_t* icon_font,
                   const lv_font_t* large_time_font,
                   std::function<void(bool)> on_visibility_changed);
    ~OledHomeScreen();

    void Initialize();

    // 显示/隐藏首页（会自动切换默认界面）
    void Show();
    void Hide();
    void Update();

    void SetWifiStatus(bool connected);
    void SetVolumeLevel(int level);  // 按键时调用，会临时显示数值
    void SetMuted(bool muted);

    bool IsVisible() const { return visible_; }
};

#endif // OLED_HOME_SCREEN_H
