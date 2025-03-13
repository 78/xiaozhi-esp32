#include "power_save_timer.h"
#include "application.h"

#include <esp_log.h>

#define TAG "PowerSaveTimer"  // 定义日志标签

// PowerSaveTimer类的构造函数
PowerSaveTimer::PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep, int seconds_to_shutdown)
    : cpu_max_freq_(cpu_max_freq), seconds_to_sleep_(seconds_to_sleep), seconds_to_shutdown_(seconds_to_shutdown) {
    // 配置定时器参数
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<PowerSaveTimer*>(arg);  // 获取PowerSaveTimer对象
            self->PowerSaveCheck();  // 调用电源节省检查函数
        },
        .arg = this,  // 传递当前对象作为回调参数
        .dispatch_method = ESP_TIMER_TASK,  // 定时器任务分发方式
        .name = "power_save_timer",  // 定时器名称
        .skip_unhandled_events = true,  // 跳过未处理的事件
    };
    // 创建定时器
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &power_save_timer_));
}

// PowerSaveTimer类的析构函数
PowerSaveTimer::~PowerSaveTimer() {
    // 停止并删除定时器
    esp_timer_stop(power_save_timer_);
    esp_timer_delete(power_save_timer_);
}

// 设置定时器启用状态的函数
void PowerSaveTimer::SetEnabled(bool enabled) {
    if (enabled && !enabled_) {
        ticks_ = 0;  // 重置计时器计数
        enabled_ = enabled;  // 更新启用状态
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));  // 启动定时器，周期为1秒
    } else if (!enabled && enabled_) {
        ESP_ERROR_CHECK(esp_timer_stop(power_save_timer_));  // 停止定时器
        enabled_ = enabled;  // 更新启用状态
    }
}

// 设置进入睡眠模式回调函数的函数
void PowerSaveTimer::OnEnterSleepMode(std::function<void()> callback) {
    on_enter_sleep_mode_ = callback;  // 保存回调函数
}

// 设置退出睡眠模式回调函数的函数
void PowerSaveTimer::OnExitSleepMode(std::function<void()> callback) {
    on_exit_sleep_mode_ = callback;  // 保存回调函数
}

// 设置关机请求回调函数的函数
void PowerSaveTimer::OnShutdownRequest(std::function<void()> callback) {
    on_shutdown_request_ = callback;  // 保存回调函数
}

// 电源节省检查函数
void PowerSaveTimer::PowerSaveCheck() {
    auto& app = Application::GetInstance();
    // 如果未进入睡眠模式且应用程序不允许进入睡眠模式，则重置计时器计数
    if (!in_sleep_mode_ && !app.CanEnterSleepMode()) {
        ticks_ = 0;
        return;
    }

    ticks_++;  // 增加计时器计数
    // 如果达到进入睡眠模式的时间阈值且未进入睡眠模式
    if (seconds_to_sleep_ != -1 && ticks_ >= seconds_to_sleep_) {
        if (!in_sleep_mode_) {
            in_sleep_mode_ = true;  // 进入睡眠模式
            if (on_enter_sleep_mode_) {
                on_enter_sleep_mode_();  // 调用进入睡眠模式回调函数
            }

            // 如果设置了最大CPU频率，则配置电源管理参数
            if (cpu_max_freq_ != -1) {
                esp_pm_config_t pm_config = {
                    .max_freq_mhz = cpu_max_freq_,  // 最大CPU频率
                    .min_freq_mhz = 40,  // 最小CPU频率
                    .light_sleep_enable = true,  // 启用轻睡眠模式
                };
                esp_pm_configure(&pm_config);  // 应用电源管理配置
            }
        }
    }
    // 如果达到关机时间阈值且设置了关机请求回调函数
    if (seconds_to_shutdown_ != -1 && ticks_ >= seconds_to_shutdown_ && on_shutdown_request_) {
        on_shutdown_request_();  // 调用关机请求回调函数
    }
}

// 唤醒设备的函数
void PowerSaveTimer::WakeUp() {
    ticks_ = 0;  // 重置计时器计数
    if (in_sleep_mode_) {
        in_sleep_mode_ = false;  // 退出睡眠模式

        // 如果设置了最大CPU频率，则恢复电源管理配置
        if (cpu_max_freq_ != -1) {
            esp_pm_config_t pm_config = {
                .max_freq_mhz = cpu_max_freq_,  // 最大CPU频率
                .min_freq_mhz = cpu_max_freq_,  // 最小CPU频率
                .light_sleep_enable = false,  // 禁用轻睡眠模式
            };
            esp_pm_configure(&pm_config);  // 应用电源管理配置
        }

        // 调用退出睡眠模式回调函数
        if (on_exit_sleep_mode_) {
            on_exit_sleep_mode_();
        }
    }
}