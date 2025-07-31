#include "sleep_timer.h"
#include "application.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_lvgl_port.h>

#define TAG "SleepTimer"


SleepTimer::SleepTimer(int seconds_to_light_sleep, int seconds_to_deep_sleep)
    : seconds_to_light_sleep_(seconds_to_light_sleep), seconds_to_deep_sleep_(seconds_to_deep_sleep) {
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<SleepTimer*>(arg);
            self->CheckTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sleep_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &sleep_timer_));
}

SleepTimer::~SleepTimer() {
    esp_timer_stop(sleep_timer_);
    esp_timer_delete(sleep_timer_);
}

void SleepTimer::SetEnabled(bool enabled) {
    if (enabled && !enabled_) {
        ticks_ = 0;
        enabled_ = enabled;
        ESP_ERROR_CHECK(esp_timer_start_periodic(sleep_timer_, 1000000));
        ESP_LOGI(TAG, "Sleep timer enabled");
    } else if (!enabled && enabled_) {
        ESP_ERROR_CHECK(esp_timer_stop(sleep_timer_));
        enabled_ = enabled;
        WakeUp();
        ESP_LOGI(TAG, "Sleep timer disabled");
    }
}

void SleepTimer::OnEnterLightSleepMode(std::function<void()> callback) {
    on_enter_light_sleep_mode_ = callback;
}

void SleepTimer::OnExitLightSleepMode(std::function<void()> callback) {
    on_exit_light_sleep_mode_ = callback;
}

void SleepTimer::OnEnterDeepSleepMode(std::function<void()> callback) {
    on_enter_deep_sleep_mode_ = callback;
}

void SleepTimer::CheckTimer() {
    auto& app = Application::GetInstance();
    if (!app.CanEnterSleepMode()) {
        ticks_ = 0;
        return;
    }

    ticks_++;
    if (seconds_to_light_sleep_ != -1 && ticks_ >= seconds_to_light_sleep_) {
        if (!in_light_sleep_mode_) {
            in_light_sleep_mode_ = true;
            if (on_enter_light_sleep_mode_) {
                on_enter_light_sleep_mode_();
            }

            auto& audio_service = app.GetAudioService();
            bool is_wake_word_running = audio_service.IsWakeWordRunning();
            if (is_wake_word_running) {
                audio_service.EnableWakeWordDetection(false);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        
            app.Schedule([this, &app]() {
                while (in_light_sleep_mode_) {
                    auto& board = Board::GetInstance();
                    board.GetDisplay()->UpdateStatusBar(true);
                    lv_refr_now(nullptr);
                    lvgl_port_stop();
    
                    // 配置timer唤醒源（30秒后自动唤醒）
                    esp_sleep_enable_timer_wakeup(30 * 1000000);
                    
                    // 进入light sleep模式
                    esp_light_sleep_start();
                    lvgl_port_resume();

                    auto wakeup_reason = esp_sleep_get_wakeup_cause();
                    ESP_LOGI(TAG, "Wake up from light sleep, wakeup_reason: %d", wakeup_reason);
                    if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
                        break;
                    }
                }
                WakeUp();
            });

            if (is_wake_word_running) {
                audio_service.EnableWakeWordDetection(true);
            }
        }
    }
    if (seconds_to_deep_sleep_ != -1 && ticks_ >= seconds_to_deep_sleep_) {
        if (on_enter_deep_sleep_mode_) {
            on_enter_deep_sleep_mode_();
        }

        esp_deep_sleep_start();
    }
}

void SleepTimer::WakeUp() {
    ticks_ = 0;
    if (in_light_sleep_mode_) {
        in_light_sleep_mode_ = false;
        if (on_exit_light_sleep_mode_) {
            on_exit_light_sleep_mode_();
        }
    }
}
