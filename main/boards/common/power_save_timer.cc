#include "power_save_timer.h"
#include "application.h"

#include <esp_log.h>

#define TAG "PowerSaveTimer"


PowerSaveTimer::PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep, int seconds_to_shutdown)
    : cpu_max_freq_(cpu_max_freq), seconds_to_sleep_(seconds_to_sleep), seconds_to_shutdown_(seconds_to_shutdown) {
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<PowerSaveTimer*>(arg);
            self->PowerSaveCheck();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "power_save_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &power_save_timer_));
}

PowerSaveTimer::~PowerSaveTimer() {
    esp_timer_stop(power_save_timer_);
    esp_timer_delete(power_save_timer_);
}

void PowerSaveTimer::SetEnabled(bool enabled) {
    if (enabled && !enabled_) {
        ticks_ = 0;
        enabled_ = enabled;
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));
        ESP_LOGI(TAG, "Power save timer enabled");
    } else if (!enabled && enabled_) {
        ESP_ERROR_CHECK(esp_timer_stop(power_save_timer_));
        enabled_ = enabled;
        WakeUp();
        ESP_LOGI(TAG, "Power save timer disabled");
    }
}

void PowerSaveTimer::OnEnterSleepMode(std::function<void()> callback) {
    on_enter_sleep_mode_ = callback;
}

void PowerSaveTimer::OnExitSleepMode(std::function<void()> callback) {
    on_exit_sleep_mode_ = callback;
}

void PowerSaveTimer::OnShutdownRequest(std::function<void()> callback) {
    on_shutdown_request_ = callback;
}

void PowerSaveTimer::PowerSaveCheck() {
    auto& app = Application::GetInstance();
    if (!in_sleep_mode_ && !app.CanEnterSleepMode()) {
        ticks_ = 0;
        return;
    }

    ticks_++;
    if (seconds_to_sleep_ != -1 && ticks_ >= seconds_to_sleep_) {
        if (!in_sleep_mode_) {
            in_sleep_mode_ = true;
            if (on_enter_sleep_mode_) {
                on_enter_sleep_mode_();
            }

            if (cpu_max_freq_ != -1) {
                esp_pm_config_t pm_config = {
                    .max_freq_mhz = cpu_max_freq_,
                    .min_freq_mhz = 40,
                    .light_sleep_enable = true,
                };
                esp_pm_configure(&pm_config);
            }
        }
    }
    if (seconds_to_shutdown_ != -1 && ticks_ >= seconds_to_shutdown_ && on_shutdown_request_) {
        on_shutdown_request_();
    }
}

void PowerSaveTimer::WakeUp() {
    ticks_ = 0;
    if (in_sleep_mode_) {
        in_sleep_mode_ = false;

        if (cpu_max_freq_ != -1) {
            esp_pm_config_t pm_config = {
                .max_freq_mhz = cpu_max_freq_,
                .min_freq_mhz = cpu_max_freq_,
                .light_sleep_enable = false,
            };
            esp_pm_configure(&pm_config);
        }

        if (on_exit_sleep_mode_) {
            on_exit_sleep_mode_();
        }
    }
}
