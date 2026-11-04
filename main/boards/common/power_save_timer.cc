#include "power_save_timer.h"
#include "application.h"
#include "settings.h"

#include <esp_log.h>

#define TAG "PowerSaveTimer"


PowerSaveTimer::PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep, int seconds_to_shutdown)
    : cpu_max_freq_(cpu_max_freq), default_seconds_to_sleep_(seconds_to_sleep),
      default_seconds_to_shutdown_(seconds_to_shutdown), seconds_to_sleep_(seconds_to_sleep),
      seconds_to_shutdown_(seconds_to_shutdown) {
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
        shutdown_requested_ = false;
        enabled_ = enabled;
        // With sleep_mode off this leaves both steps disabled (-1), but the
        // timer still runs so enabling it later in the web config applies
        // without a reboot.
        ApplySettings();
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));
        ESP_LOGI(TAG, "Power save timer enabled (sleep=%d s, shutdown=%d s)", seconds_to_sleep_, seconds_to_shutdown_);
    } else if (!enabled && enabled_) {
        ESP_ERROR_CHECK(esp_timer_stop(power_save_timer_));
        enabled_ = enabled;
        WakeUp();
        ESP_LOGI(TAG, "Power save timer disabled");
    }
}

// Re-read the sleep timer values from settings. NVS keys (minutes) win over the
// board's constructor defaults; 0 disables that step; when sleep_mode is off
// both steps are disabled. A changed value restarts the idle countdown.
void PowerSaveTimer::ApplySettings() {
    Settings settings("wifi", false);
    int eff_sleep = -1;
    int eff_shutdown = -1;
    if (settings.GetBool("sleep_mode", true)) {
        eff_sleep = default_seconds_to_sleep_;
        eff_shutdown = default_seconds_to_shutdown_;
        // NVS keys are limited to 15 characters.
        int dim_minutes = settings.GetInt("sleep_dim_min", -1);
        if (dim_minutes >= 0) {
            eff_sleep = dim_minutes > 0 ? dim_minutes * 60 : -1;
        }
        int off_minutes = settings.GetInt("sleep_off_min", -1);
        if (off_minutes >= 0) {
            eff_shutdown = off_minutes > 0 ? off_minutes * 60 : -1;
        }
    }
    if (eff_sleep != seconds_to_sleep_ || eff_shutdown != seconds_to_shutdown_) {
        ESP_LOGI(TAG, "Sleep timers updated: sleep=%d s, shutdown=%d s", eff_sleep, eff_shutdown);
        seconds_to_sleep_ = eff_sleep;
        seconds_to_shutdown_ = eff_shutdown;
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
    // Pick up sleep timer changes saved through the web config without a reboot.
    ApplySettings();

    auto& app = Application::GetInstance();
    if (!in_sleep_mode_ && !app.CanEnterSleepMode()) {
        if (ticks_ > 0) {
            ESP_LOGI(TAG, "Activity detected, power save timer reset");
        }
        ticks_ = 0;
        shutdown_requested_ = false;
        return;
    }

    ticks_++;
    if (seconds_to_sleep_ != -1 && ticks_ >= seconds_to_sleep_) {
        if (!in_sleep_mode_) {
            ESP_LOGI(TAG, "Enabling power save mode");
            in_sleep_mode_ = true;
            if (on_enter_sleep_mode_) {
                on_enter_sleep_mode_();
            }

            if (cpu_max_freq_ != -1) {
                // Disable wake word detection
                auto& audio_service = app.GetAudioService();
                is_wake_word_running_ = audio_service.IsWakeWordRunning();
                if (is_wake_word_running_) {
                    audio_service.EnableWakeWordDetection(false);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                // Disable audio input
                auto codec = Board::GetInstance().GetAudioCodec();
                if (codec) {
                    codec->EnableInput(false);
                }

                esp_pm_config_t pm_config = {
                    .max_freq_mhz = cpu_max_freq_,
                    .min_freq_mhz = 40,
                    .light_sleep_enable = true,
                };
                esp_pm_configure(&pm_config);
            }
        }
    }
    // Latch before invoking the callback so it can rearm the request with WakeUp().
    if (seconds_to_shutdown_ != -1 && ticks_ >= seconds_to_shutdown_ && on_shutdown_request_ &&
        !shutdown_requested_) {
        shutdown_requested_ = true;
        on_shutdown_request_();
    }
}

void PowerSaveTimer::WakeUp() {
    ticks_ = 0;
    shutdown_requested_ = false;
    if (in_sleep_mode_) {
        ESP_LOGI(TAG, "Exiting power save mode");
        in_sleep_mode_ = false;

        if (cpu_max_freq_ != -1) {
            esp_pm_config_t pm_config = {
                .max_freq_mhz = cpu_max_freq_,
                .min_freq_mhz = cpu_max_freq_,
                .light_sleep_enable = false,
            };
            esp_pm_configure(&pm_config);

            // Enable wake word detection
            auto& app = Application::GetInstance();
            auto& audio_service = app.GetAudioService();
            if (is_wake_word_running_) {
                audio_service.EnableWakeWordDetection(true);
            }
        }

        if (on_exit_sleep_mode_) {
            on_exit_sleep_mode_();
        }
    }
}
