#include "oled_home_screen.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <time.h>
#include <sys/time.h>
#include <cstdio>

#define TAG "OledHomeScreen"

OledHomeScreen::OledHomeScreen(int width, int height,
                               const lv_font_t* small_font,
                               const lv_font_t* icon_font,
                               const lv_font_t* large_time_font,
                               std::function<void(bool)> on_visibility_changed)
    : width_(width), height_(height),
      small_font_(small_font), icon_font_(icon_font),
      large_time_font_(large_time_font),
      on_visibility_changed_(on_visibility_changed) {
    is_128x64_ = (height >= 64);
}

OledHomeScreen::~OledHomeScreen() {
    if (update_timer_) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }
    if (volume_text_timer_) {
        esp_timer_stop(volume_text_timer_);
        esp_timer_delete(volume_text_timer_);
    }
    if (screen_) {
        lvgl_port_lock(0);
        lv_obj_del(screen_);
        lvgl_port_unlock();
    }
}

void OledHomeScreen::Initialize() {
    lvgl_port_lock(0);

    screen_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen_, width_, height_);
    lv_obj_set_style_bg_color(screen_, lv_color_white(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_style_pad_all(screen_, 0, 0);
    lv_obj_align(screen_, LV_ALIGN_TOP_LEFT, 0, 0);

    if (is_128x64_) {
        // 128x64 布局：两栏结构
        // 顶部状态栏: 0-16px  (16px)
        // 主时间区:   16-64px (48px)
        CreateStatusBar();
        CreateMainTimeArea();
    } else {
        // 128x32 布局：单栏紧凑结构
        CreateStatusBar();
    }

    // 默认隐藏，等待状态检查后决定是否显示
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();

    // 创建更新定时器（每秒更新一次）
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            static_cast<OledHomeScreen*>(arg)->Update();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "home_screen_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timer_args, &update_timer_);
    esp_timer_start_periodic(update_timer_, 1000000);

    // 创建音量文字自动隐藏定时器（3秒后隐藏）
    esp_timer_create_args_t vol_timer_args = {
        .callback = [](void* arg) {
            static_cast<OledHomeScreen*>(arg)->HideVolumeText();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "vol_text_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&vol_timer_args, &volume_text_timer_);

    ESP_LOGI(TAG, "Home screen initialized (%dx%d)", width_, height_);
}

void OledHomeScreen::CreateStatusBar() {
    status_bar_ = lv_obj_create(screen_);

    int bar_height = is_128x64_ ? 16 : height_;
    lv_obj_set_size(status_bar_, width_, bar_height);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_style_bg_color(status_bar_, lv_color_white(), 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_hor(status_bar_, 3, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_bar_, 2, 0);

    // === 左侧：WiFi 图标 ===
    lv_obj_t* left_group = lv_obj_create(status_bar_);
    lv_obj_set_size(left_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_group, 0, 0);
    lv_obj_set_style_pad_all(left_group, 0, 0);
    lv_obj_set_flex_flow(left_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left_group, 2, 0);

    wifi_icon_ = lv_label_create(left_group);
    lv_label_set_text(wifi_icon_, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon_, icon_font_, 0);
    lv_obj_set_style_text_color(wifi_icon_, lv_color_black(), 0);

    // === 中间：状态文字 "待命" ===
    status_label_ = lv_label_create(status_bar_);
    lv_label_set_text(status_label_, "待命");
    lv_obj_set_style_text_font(status_label_, small_font_, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_make(75, 75, 75), 0);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    // === 右侧：音量图标 + 音量数值 ===
    lv_obj_t* right_group = lv_obj_create(status_bar_);
    lv_obj_set_size(right_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_group, 0, 0);
    lv_obj_set_style_pad_all(right_group, 0, 0);
    lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_group, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right_group, 2, 0);

    // 音量图标
    volume_icon_ = lv_label_create(right_group);
    lv_label_set_text(volume_icon_, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(volume_icon_, icon_font_, 0);
    lv_obj_set_style_text_color(volume_icon_, lv_color_black(), 0);

    // 音量数值文字（默认隐藏，按键时显示）
    volume_text_ = lv_label_create(right_group);
    lv_label_set_text(volume_text_, "--");
    lv_obj_set_style_text_font(volume_text_, small_font_, 0);
    lv_obj_set_style_text_color(volume_text_, lv_color_black(), 0);
    lv_obj_add_flag(volume_text_, LV_OBJ_FLAG_HIDDEN);
}

void OledHomeScreen::CreateMainTimeArea() {
    if (!is_128x64_) return;

    // 主时间区：19px-64px，高度45px
    // 放在状态栏下方，下移3像素
    main_time_area_ = lv_obj_create(screen_);
    lv_obj_set_size(main_time_area_, width_, 45);
    lv_obj_align(main_time_area_, LV_ALIGN_TOP_MID, 0, 19);
    lv_obj_set_style_bg_opa(main_time_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_time_area_, 0, 0);
    lv_obj_set_style_pad_all(main_time_area_, 0, 0);
    lv_obj_set_scrollbar_mode(main_time_area_, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_flex_flow(main_time_area_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_time_area_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(main_time_area_, 0, 0);

    // 大时间显示 - 居中，字体尽量大
    large_time_label_ = lv_label_create(main_time_area_);
    lv_label_set_text(large_time_label_, "--:--:--");
    lv_obj_set_style_text_font(large_time_label_, large_time_font_, 0);
    lv_obj_set_style_text_color(large_time_label_, lv_color_black(), 0);
    lv_obj_set_style_text_letter_space(large_time_label_, 1, 0);

    // 日期显示 - 大时间下方
    date_label_ = lv_label_create(main_time_area_);
    lv_label_set_text(date_label_, "----/--/--");
    lv_obj_set_style_text_font(date_label_, small_font_, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_make(75, 75, 75), 0);
}

void OledHomeScreen::UpdateTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 检查时间是否有效（年份大于2020表示NTP已同步）
    if (timeinfo.tm_year + 1900 < 2020) {
        return;  // 时间未同步，不更新
    }

    char time_str[16];

    lvgl_port_lock(0);

    if (is_128x64_ && large_time_label_) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        lv_label_set_text(large_time_label_, time_str);
    }

    lvgl_port_unlock();
}

void OledHomeScreen::UpdateDate() {
    if (!is_128x64_) return;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year + 1900 < 2020) {
        return;  // 时间未同步
    }

    lvgl_port_lock(0);

    if (date_label_) {
        char date_str[32];
        const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(date_str, sizeof(date_str), "%04d/%02d/%02d %s",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                weekdays[timeinfo.tm_wday]);
        lv_label_set_text(date_label_, date_str);
    }

    lvgl_port_unlock();
}

void OledHomeScreen::Update() {
    UpdateTime();

    // 每分钟更新一次日期
    static int last_minute = -1;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_min != last_minute) {
        last_minute = timeinfo.tm_min;
        UpdateDate();
    }
}

void OledHomeScreen::SetVisible(bool visible) {
    if (visible_ == visible) return;
    visible_ = visible;

    lvgl_port_lock(0);
    if (visible) {
        if (screen_) {
            lv_obj_clear_flag(screen_, LV_OBJ_FLAG_HIDDEN);
        }
        // 通知回调：隐藏默认界面
        if (on_visibility_changed_) {
            lvgl_port_unlock();
            on_visibility_changed_(true);
            return;
        }
    } else {
        if (screen_) {
            lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
        }
        // 通知回调：显示默认界面
        if (on_visibility_changed_) {
            lvgl_port_unlock();
            on_visibility_changed_(false);
            return;
        }
    }
    lvgl_port_unlock();
}

void OledHomeScreen::Show() {
    SetVisible(true);
    Update();
}

void OledHomeScreen::Hide() {
    SetVisible(false);
}

void OledHomeScreen::SetWifiStatus(bool connected) {
    if (!wifi_icon_) return;

    lvgl_port_lock(0);

    if (connected) {
        lv_label_set_text(wifi_icon_, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon_, lv_color_black(), 0);
    } else {
        lv_label_set_text(wifi_icon_, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(wifi_icon_, lv_color_make(160, 160, 160), 0);
    }

    lvgl_port_unlock();
}

void OledHomeScreen::ShowVolumeText(int level) {
    if (!volume_text_) return;

    lvgl_port_lock(0);

    char vol_str[8];
    if (level == 0) {
        snprintf(vol_str, sizeof(vol_str), "M");
    } else {
        snprintf(vol_str, sizeof(vol_str), "%d", level);
    }
    lv_label_set_text(volume_text_, vol_str);
    lv_obj_clear_flag(volume_text_, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();

    // 重启3秒隐藏定时器
    if (volume_text_timer_) {
        esp_timer_stop(volume_text_timer_);
        esp_timer_start_once(volume_text_timer_, 3000000);  // 3秒
    }
}

void OledHomeScreen::HideVolumeText() {
    if (!volume_text_) return;

    lvgl_port_lock(0);
    lv_obj_add_flag(volume_text_, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

void OledHomeScreen::SetVolumeLevel(int level) {
    lvgl_port_lock(0);

    if (volume_icon_) {
        if (level == 0) {
            lv_label_set_text(volume_icon_, LV_SYMBOL_MUTE);
        } else if (level < 50) {
            lv_label_set_text(volume_icon_, LV_SYMBOL_VOLUME_MID);
        } else {
            lv_label_set_text(volume_icon_, LV_SYMBOL_VOLUME_MAX);
        }
        lv_obj_set_style_text_color(volume_icon_, lv_color_black(), 0);
    }

    lvgl_port_unlock();

    // 显示音量数值3秒
    ShowVolumeText(level);
}

void OledHomeScreen::SetMuted(bool muted) {
    if (muted) {
        lvgl_port_lock(0);

        if (volume_icon_) {
            lv_label_set_text(volume_icon_, LV_SYMBOL_MUTE);
            lv_obj_set_style_text_color(volume_icon_, lv_color_make(160, 160, 160), 0);
        }

        lvgl_port_unlock();

        ShowVolumeText(0);
    }
}
