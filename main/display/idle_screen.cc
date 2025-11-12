#include "idle_screen.h"
#include "display.h"
#include "application.h"

#include <esp_log.h>
#include <cstring>  // for strcmp

#ifdef CONFIG_ENABLE_IDLE_SCREEN
#include <time.h>
#include <sys/time.h>

#ifdef HAVE_LVGL
#include "lvgl_theme.h"
#endif

// 包含项目字体作为后备
extern "C" {
    extern const lv_font_t BUILTIN_TEXT_FONT;
    // 引入番茄计时器商标图片资源
    LV_IMG_DECLARE(_tomatotimers_RGB565A8_500x220);
}
#endif

#define TAG "IdleScreen"

#ifdef CONFIG_ENABLE_IDLE_SCREEN

// ============= 启用待机界面功能时的完整实现 =============

IdleScreen::IdleScreen(Display* display)
    : display_(display),
      is_active_(false),
      is_enabled_(false),
      last_activity_time_(std::chrono::system_clock::now()),
      idle_container_(nullptr),
      background_img_(nullptr),
      logo_img_(nullptr),
      time_label_(nullptr),
      weekday_label_(nullptr),
      date_label_(nullptr),
      progress_bar_(nullptr) {
    
    // 创建待机检测定时器（每秒检查一次）
    esp_timer_create_args_t idle_timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<IdleScreen*>(arg);
            self->CheckIdleTimeout();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "idle_screen_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&idle_timer_args, &idle_timer_));
    
    // 创建界面更新定时器（每秒更新一次）
    esp_timer_create_args_t update_timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<IdleScreen*>(arg);
            if (self->is_active_) {
                self->UpdateDisplay();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "idle_screen_update",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_timer_args, &update_timer_));
    
    ESP_LOGI(TAG, "IdleScreen initialized, timeout: %d seconds (auto-tracked)", kIdleTimeoutSeconds);
}

IdleScreen::~IdleScreen() {
    Stop();
    
    if (idle_timer_ != nullptr) {
        esp_timer_stop(idle_timer_);
        esp_timer_delete(idle_timer_);
    }
    
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }
    
    DestroyIdleScreenUI();
}

void IdleScreen::Start() {
    if (!is_enabled_) {
        is_enabled_ = true;
        last_activity_time_ = std::chrono::system_clock::now();  // 初始化活动时间
        ESP_ERROR_CHECK(esp_timer_start_periodic(idle_timer_, 1000000));  // 每秒
        ESP_LOGI(TAG, "IdleScreen started");
    }
}

void IdleScreen::Stop() {
    if (is_enabled_) {
        is_enabled_ = false;
        esp_timer_stop(idle_timer_);
        esp_timer_stop(update_timer_);
        HideIdleScreen();
        ESP_LOGI(TAG, "IdleScreen stopped");
    }
}

void IdleScreen::ResetTimer() {
    last_activity_time_ = std::chrono::system_clock::now();
    
    if (is_active_) {
        HideIdleScreen();
    }
}

void IdleScreen::CheckIdleTimeout() {
    auto& app = Application::GetInstance();
    
    // 检查设备是否处于空闲状态
    if (app.GetDeviceState() != kDeviceStateIdle) {
        last_activity_time_ = std::chrono::system_clock::now();
        if (is_active_) {
            HideIdleScreen();
        }
        return;
    }
    
    // 计算距离上次活动的时间（参考状态栏时间显示的实现）
    auto now = std::chrono::system_clock::now();
    auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_time_).count();
    
    // 达到超时时间（10秒），显示待机界面
    // 与状态栏时间显示条件保持一致
    if (!is_active_ && idle_duration >= kIdleTimeoutSeconds) {
        // 检查系统时间是否已同步（年份 >= 2025）
        time_t now_time;
        struct tm timeinfo;
        time(&now_time);
        localtime_r(&now_time, &timeinfo);
        
        if (timeinfo.tm_year >= 2025 - 1900) {
            ShowIdleScreen();
        } else {
            ESP_LOGD(TAG, "System time not synced yet, skip showing idle screen (idle: %lld seconds)", idle_duration);
        }
    }
}

void IdleScreen::ShowIdleScreen() {
    if (is_active_) {
        return;
    }
    
    ESP_LOGI(TAG, "Showing idle screen");
    is_active_ = true;
    
    CreateIdleScreenUI();
    UpdateDisplay();
    
    // 启动界面更新定时器
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer_, 1000000));  // 每秒更新
}

void IdleScreen::HideIdleScreen() {
    if (!is_active_) {
        return;
    }
    
    ESP_LOGI(TAG, "Hiding idle screen");
    is_active_ = false;
    last_activity_time_ = std::chrono::system_clock::now();
    
    // 停止更新定时器
    esp_timer_stop(update_timer_);
    
    DestroyIdleScreenUI();
}

void IdleScreen::CreateIdleScreenUI() {
    if (idle_container_ != nullptr) {
        return;  // 已经创建
    }

    DisplayLockGuard lock(display_);

    auto screen = lv_screen_active();

    // 创建全屏容器 - 使用粉色背景
    idle_container_ = lv_obj_create(screen);
    lv_obj_set_size(idle_container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(idle_container_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(idle_container_, lv_color_hex(0xFFC0CB), 0);  // 粉色背景
    lv_obj_set_style_bg_opa(idle_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(idle_container_, 0, 0);
    lv_obj_set_style_pad_all(idle_container_, 0, 0);
    lv_obj_set_style_radius(idle_container_, 0, 0);
    lv_obj_clear_flag(idle_container_, LV_OBJ_FLAG_SCROLLABLE);

    // 1. 创建番茄计时器商标图片（顶部）
    logo_img_ = lv_img_create(idle_container_);
    lv_obj_set_pos(logo_img_, 20, 10);
    lv_obj_set_size(logo_img_, 200, 88);
    lv_img_set_src(logo_img_, &_tomatotimers_RGB565A8_500x220);
    lv_obj_set_style_img_opa(logo_img_, LV_OPA_80, 0);  // 80% 不透明度

    // 2. 创建时间显示容器（白色半透明背景）
    lv_obj_t *time_container = lv_obj_create(idle_container_);
    lv_obj_set_size(time_container, 220, 100);
    lv_obj_set_pos(time_container, 10, 95);
    lv_obj_set_style_bg_color(time_container, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(time_container, LV_OPA_90, 0);
    lv_obj_set_style_radius(time_container, 20, 0);
    lv_obj_set_style_border_width(time_container, 2, 0);
    lv_obj_set_style_border_color(time_container, lv_color_hex(0xFFB6C1), 0);
    lv_obj_set_style_pad_all(time_container, 0, 0);
    lv_obj_clear_flag(time_container, LV_OBJ_FLAG_SCROLLABLE);

    // 3. 创建时间标签（使用flex布局实现居中，无需手动计算padding）
    time_label_ = lv_label_create(time_container);
    lv_obj_set_size(time_label_, LV_PCT(100), LV_SIZE_CONTENT);  // 宽度100%容器，高度自适应
    lv_obj_set_flex_align(time_label_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(0xFF69B4), 0);
    lv_obj_set_style_text_letter_space(time_label_, 15, 0);
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(time_label_, LV_OPA_TRANSP, 0);
    lv_label_set_text(time_label_, "00:00");
    // 启用flex布局（更现代的居中方式）
    lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 4. 创建星期标签（中文字体，20px，粉色半透明背景）
    weekday_label_ = lv_label_create(idle_container_);
    lv_obj_set_size(weekday_label_, 120, 30);
    lv_obj_align(weekday_label_, LV_ALIGN_CENTER, -50, 50);
    lv_obj_set_style_text_color(weekday_label_, lv_color_hex(0xFFFFFF), 0);  // 白色文字
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(weekday_label_, lv_color_hex(0xFF69B4), 0);  // 粉色背景
    lv_obj_set_style_bg_opa(weekday_label_, LV_OPA_70, 0);  // 70% 不透明度
    lv_obj_set_style_radius(weekday_label_, 15, 0);
    lv_obj_set_style_pad_all(weekday_label_, 4, 0);
    // 优先使用主题中的动态字体，回退到内置字体
    const lv_font_t* weekday_font = &BUILTIN_TEXT_FONT;
#ifdef HAVE_LVGL
    {
        auto& theme_manager = LvglThemeManager::GetInstance();
        auto theme = theme_manager.GetTheme("light");
        if (theme == nullptr) {
            theme = theme_manager.GetTheme("dark");
        }
        if (theme != nullptr && theme->text_font() != nullptr) {
            weekday_font = theme->text_font()->font();
        }
    }
#endif
    lv_obj_set_style_text_font(weekday_label_, weekday_font, 0);
    lv_label_set_text(weekday_label_, "星期一");

    // 5. 创建日期标签（20px，天蓝色半透明背景）
    date_label_ = lv_label_create(idle_container_);
    lv_obj_set_size(date_label_, 120, 30);
    lv_obj_align(date_label_, LV_ALIGN_CENTER, 50, 50);
    // 使用 LVGL 字体族中的 20px 字体
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(0xFFFFFF), 0);  // 白色文字
    lv_obj_set_style_text_align(date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(date_label_, lv_color_hex(0x87CEEB), 0);  // 天蓝色背景
    lv_obj_set_style_bg_opa(date_label_, LV_OPA_70, 0);  // 70% 不透明度
    lv_obj_set_style_radius(date_label_, 15, 0);
    lv_obj_set_style_pad_all(date_label_, 5, 0);
    lv_label_set_text(date_label_, "01-24");

    // 6. 创建进度条（底部，粉色）
    progress_bar_ = lv_bar_create(idle_container_);
    lv_obj_set_size(progress_bar_, 180, 8);
    lv_obj_align(progress_bar_, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_bar_set_range(progress_bar_, 0, 60);
    lv_bar_set_value(progress_bar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(progress_bar_, 4, 0);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0xFFFFFF), 0);  // 白色背景
    lv_obj_set_style_bg_opa(progress_bar_, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0xFF69B4), LV_PART_INDICATOR);  // 粉色指示器
    lv_obj_set_style_bg_opa(progress_bar_, LV_OPA_COVER, LV_PART_INDICATOR);

    ESP_LOGI(TAG, "Idle screen UI created (TomatoTimers style with pink background)");
}

void IdleScreen::DestroyIdleScreenUI() {
    if (idle_container_ != nullptr) {
        DisplayLockGuard lock(display_);
        // 删除容器会自动删除所有子对象
        lv_obj_del(idle_container_);
        idle_container_ = nullptr;
        background_img_ = nullptr;
        logo_img_ = nullptr;
        time_label_ = nullptr;
        weekday_label_ = nullptr;
        date_label_ = nullptr;
        progress_bar_ = nullptr;
        ESP_LOGI(TAG, "Idle screen UI destroyed");
    }
}

void IdleScreen::UpdateDisplay() {
    if (!is_active_ || idle_container_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(display_);

    // 获取当前时间
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // ✅ 使用静态缓冲区（避免临时对象生命周期问题）
    static char time_buf[16];
    static char date_buf[16];

    // 格式化时间（HH:MM 格式）
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);

    // 更新时间标签
    lv_label_set_text(time_label_, time_buf);

    // 更新日期（MM-DD 格式）
    snprintf(date_buf, sizeof(date_buf), "%02d-%02d",
             timeinfo.tm_mon + 1, timeinfo.tm_mday);
    lv_label_set_text(date_label_, date_buf);

    // 更新星期
    std::string weekday_str = GetWeekDay();
    lv_label_set_text(weekday_label_, weekday_str.c_str());

    // 更新进度条（同步当前秒数）
    if (progress_bar_ != nullptr) {
        lv_bar_set_value(progress_bar_, timeinfo.tm_sec, LV_ANIM_OFF);
    }
}

std::string IdleScreen::GetCurrentTime() {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 格式化时间：13:45
    strftime(strftime_buf, sizeof(strftime_buf), "%H:%M", &timeinfo);
    
    return std::string(strftime_buf);
}

std::string IdleScreen::GetCurrentDate() {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 格式化日期：2025-01-14
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d", &timeinfo);
    
    return std::string(strftime_buf);
}

std::string IdleScreen::GetWeekDay() {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 中文星期
    const char* weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    
    std::string weekday = weekdays[timeinfo.tm_wday];
    // ESP_LOGI(TAG, "Current weekday: %s (wday=%d)", weekday.c_str(), timeinfo.tm_wday);
    
    return weekday;
}

#else // CONFIG_ENABLE_IDLE_SCREEN 未定义 - 提供空实现

// ============= 禁用待机界面功能时的空实现 =============

IdleScreen::IdleScreen(Display* display)
    : display_(display),
      is_active_(false),
      is_enabled_(false),
      last_activity_time_(std::chrono::system_clock::now()),
      idle_container_(nullptr),
      background_img_(nullptr),
      logo_img_(nullptr),
      time_label_(nullptr),
      weekday_label_(nullptr),
      date_label_(nullptr),
      progress_bar_(nullptr) {
    ESP_LOGI(TAG, "IdleScreen feature is disabled (CONFIG_ENABLE_IDLE_SCREEN not set)");
}

IdleScreen::~IdleScreen() {}

void IdleScreen::Start() {}

void IdleScreen::Stop() {}

void IdleScreen::ResetTimer() {}

void IdleScreen::CheckIdleTimeout() {}

void IdleScreen::ShowIdleScreen() {}

void IdleScreen::HideIdleScreen() {}

void IdleScreen::UpdateDisplay() {}

void IdleScreen::CreateIdleScreenUI() {}

void IdleScreen::DestroyIdleScreenUI() {}

std::string IdleScreen::GetCurrentTime() { return "00:00"; }

std::string IdleScreen::GetCurrentDate() { return "2025-01-01"; }

std::string IdleScreen::GetWeekDay() { return ""; }

#endif // CONFIG_ENABLE_IDLE_SCREEN
