#pragma once

#include <functional>
#include <esp_timer.h>
#include <string>
#include <chrono>

#ifdef CONFIG_ENABLE_IDLE_SCREEN
#include <lvgl.h>
#endif

class Display;

class IdleScreen {
public:
    IdleScreen(Display* display);
    ~IdleScreen();

    void Start();
    void Stop();
    void ResetTimer();  // 重置待机计时器（有用户活动时调用）
    bool IsActive() const { return is_active_; }

private:
    void CheckIdleTimeout();
    void ShowIdleScreen();
    void HideIdleScreen();
    void UpdateDisplay();
    
    void CreateIdleScreenUI();
    void DestroyIdleScreenUI();
    
    std::string GetCurrentTime();
    std::string GetCurrentDate();
    std::string GetWeekDay();

    Display* display_;
    esp_timer_handle_t idle_timer_;
    esp_timer_handle_t update_timer_;
    
    bool is_active_;
    bool is_enabled_;
    std::chrono::system_clock::time_point last_activity_time_;  // 上次用户活动时间
    
    static constexpr int kIdleTimeoutSeconds = 10;  // 固定 10 秒超时，与状态栏时间显示一致
    
    // LVGL UI objects
#ifdef CONFIG_ENABLE_IDLE_SCREEN
    lv_obj_t* idle_container_;
    lv_obj_t* background_img_;    // 背景图片（从 Assets 下载）
    lv_obj_t* logo_img_;          // Logo 图片
    lv_obj_t* time_label_;
    lv_obj_t* weekday_label_;
    lv_obj_t* date_label_;
    lv_obj_t* progress_bar_;      // 进度条
#else
    void* idle_container_;
    void* background_img_;
    void* logo_img_;
    void* time_label_;
    void* weekday_label_;
    void* date_label_;
    void* progress_bar_;
#endif
};

