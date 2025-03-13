#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>

#include "display.h"
#include "board.h"
#include "application.h"
#include "font_awesome_symbols.h"
#include "audio_codec.h"
#include "settings.h"

#define TAG "Display"  // 定义日志标签

// Display类的构造函数
Display::Display() {
    // 创建通知定时器
    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg) {
            // 定时器回调函数，用于隐藏通知标签并显示状态标签
            Display *display = static_cast<Display*>(arg);
            DisplayLockGuard lock(display);  // 加锁确保线程安全
            lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏通知标签
            lv_obj_clear_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN);  // 显示状态标签
        },
        .arg = this,  // 传递当前对象的指针作为回调参数
        .dispatch_method = ESP_TIMER_TASK,  // 定时器任务调度方法
        .name = "notification_timer",  // 定时器名称
        .skip_unhandled_events = false,  // 不跳过未处理的事件
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));  // 创建定时器

    // 创建显示更新定时器
    esp_timer_create_args_t update_display_timer_args = {
        .callback = [](void *arg) {
            // 定时器回调函数，用于定期更新显示内容
            Display *display = static_cast<Display*>(arg);
            display->Update();  // 调用更新函数
        },
        .arg = this,  // 传递当前对象的指针作为回调参数
        .dispatch_method = ESP_TIMER_TASK,  // 定时器任务调度方法
        .name = "display_update_timer",  // 定时器名称
        .skip_unhandled_events = true,  // 跳过未处理的事件
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_display_timer_args, &update_timer_));  // 创建定时器
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer_, 1000000));  // 启动定时器，每秒触发一次

    // 创建电源管理锁
    auto ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "display_update", &pm_lock_);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "Power management not supported");  // 如果电源管理不支持，记录日志
    } else {
        ESP_ERROR_CHECK(ret);  // 检查并处理错误
    }
}

// Display类的析构函数
Display::~Display() {
    // 停止并删除通知定时器
    if (notification_timer_ != nullptr) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }
    // 停止并删除显示更新定时器
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }

    // 删除所有LVGL对象
    if (network_label_ != nullptr) {
        lv_obj_del(network_label_);
        lv_obj_del(notification_label_);
        lv_obj_del(status_label_);
        lv_obj_del(mute_label_);
        lv_obj_del(battery_label_);
        lv_obj_del(emotion_label_);
    }

    // 删除电源管理锁
    if (pm_lock_ != nullptr) {
        esp_pm_lock_delete(pm_lock_);
    }
}

// 设置状态标签的文本
void Display::SetStatus(const char* status) {
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (status_label_ == nullptr) {
        return;  // 如果状态标签未初始化，直接返回
    }
    lv_label_set_text(status_label_, status);  // 设置状态标签的文本
    lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);  // 显示状态标签
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏通知标签
}

// 显示通知，接受std::string类型的通知内容和持续时间
void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);  // 调用重载函数
}

// 显示通知，接受C风格字符串的通知内容和持续时间
void Display::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (notification_label_ == nullptr) {
        return;  // 如果通知标签未初始化，直接返回
    }
    lv_label_set_text(notification_label_, notification);  // 设置通知标签的文本
    lv_obj_clear_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  // 显示通知标签
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏状态标签

    esp_timer_stop(notification_timer_);  // 停止之前的定时器
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));  // 启动定时器，设置持续时间
}

// 更新显示内容
void Display::Update() {
    auto& board = Board::GetInstance();  // 获取Board单例
    auto codec = board.GetAudioCodec();  // 获取音频编解码器

    {
        DisplayLockGuard lock(this);  // 加锁确保线程安全
        if (mute_label_ == nullptr) {
            return;  // 如果静音标签未初始化，直接返回
        }

        // 如果静音状态改变，则更新图标
        if (codec->output_volume() == 0 && !muted_) {
            muted_ = true;  // 设置静音状态为true
            lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);  // 设置静音图标
        } else if (codec->output_volume() > 0 && muted_) {
            muted_ = false;  // 设置静音状态为false
            lv_label_set_text(mute_label_, "");  // 清除静音图标
        }
    }

    esp_pm_lock_acquire(pm_lock_);  // 获取电源管理锁

    // 更新电池图标
    int battery_level;
    bool charging;
    const char* icon = nullptr;
    if (board.GetBatteryLevel(battery_level, charging)) {  // 获取电池电量和充电状态
        if (charging) {
            icon = FONT_AWESOME_BATTERY_CHARGING;  // 如果正在充电，显示充电图标
        } else {
            const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_1,    // 20-39%
                FONT_AWESOME_BATTERY_2,    // 40-59%
                FONT_AWESOME_BATTERY_3,    // 60-79%
                FONT_AWESOME_BATTERY_FULL, // 80-99%
                FONT_AWESOME_BATTERY_FULL, // 100%
            };
            icon = levels[battery_level / 20];  // 根据电量百分比选择对应的电池图标
        }
        DisplayLockGuard lock(this);  // 加锁确保线程安全
        if (battery_label_ != nullptr && battery_icon_ != icon) {
            battery_icon_ = icon;  // 更新电池图标
            lv_label_set_text(battery_label_, battery_icon_);  // 设置电池标签的图标
        }
    }

    // 升级固件时，不读取4G网络状态，避免占用UART资源
    auto device_state = Application::GetInstance().GetDeviceState();  // 获取设备状态
    static const std::vector<DeviceState> allowed_states = {
        kDeviceStateIdle,
        kDeviceStateStarting,
        kDeviceStateWifiConfiguring,
        kDeviceStateListening,
    };
    if (std::find(allowed_states.begin(), allowed_states.end(), device_state) != allowed_states.end()) {
        icon = board.GetNetworkStateIcon();  // 获取网络状态图标
        if (network_label_ != nullptr && icon != nullptr && network_icon_ != icon) {
            DisplayLockGuard lock(this);  // 加锁确保线程安全
            network_icon_ = icon;  // 更新网络图标
            lv_label_set_text(network_label_, network_icon_);  // 设置网络标签的图标
        }
    }

    esp_pm_lock_release(pm_lock_);  // 释放电源管理锁
}

// 设置表情图标
void Display::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;  // 表情图标
        const char* text;  // 表情文本
    };

    // 定义所有支持的表情
    static const std::vector<Emotion> emotions = {
        {FONT_AWESOME_EMOJI_NEUTRAL, "neutral"},
        {FONT_AWESOME_EMOJI_HAPPY, "happy"},
        {FONT_AWESOME_EMOJI_LAUGHING, "laughing"},
        {FONT_AWESOME_EMOJI_FUNNY, "funny"},
        {FONT_AWESOME_EMOJI_SAD, "sad"},
        {FONT_AWESOME_EMOJI_ANGRY, "angry"},
        {FONT_AWESOME_EMOJI_CRYING, "crying"},
        {FONT_AWESOME_EMOJI_LOVING, "loving"},
        {FONT_AWESOME_EMOJI_EMBARRASSED, "embarrassed"},
        {FONT_AWESOME_EMOJI_SURPRISED, "surprised"},
        {FONT_AWESOME_EMOJI_SHOCKED, "shocked"},
        {FONT_AWESOME_EMOJI_THINKING, "thinking"},
        {FONT_AWESOME_EMOJI_WINKING, "winking"},
        {FONT_AWESOME_EMOJI_COOL, "cool"},
        {FONT_AWESOME_EMOJI_RELAXED, "relaxed"},
        {FONT_AWESOME_EMOJI_DELICIOUS, "delicious"},
        {FONT_AWESOME_EMOJI_KISSY, "kissy"},
        {FONT_AWESOME_EMOJI_CONFIDENT, "confident"},
        {FONT_AWESOME_EMOJI_SLEEPY, "sleepy"},
        {FONT_AWESOME_EMOJI_SILLY, "silly"},
        {FONT_AWESOME_EMOJI_CONFUSED, "confused"}
    };
    
    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });
    
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (emotion_label_ == nullptr) {
        return;  // 如果表情标签未初始化，直接返回
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);  // 设置表情标签的图标
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_EMOJI_NEUTRAL);  // 设置默认表情图标
    }
}

// 设置图标
void Display::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (emotion_label_ == nullptr) {
        return;  // 如果表情标签未初始化，直接返回
    }
    lv_label_set_text(emotion_label_, icon);  // 设置表情标签的图标
}

// 设置聊天消息
void Display::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (chat_message_label_ == nullptr) {
        return;  // 如果聊天消息标签未初始化，直接返回
    }
    lv_label_set_text(chat_message_label_, content);  // 设置聊天消息标签的内容
}