#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

class PowerSaveTimer {
public:
    PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep = 20, int seconds_to_shutdown = -1);
    ~PowerSaveTimer();

    // A disabled-to-enabled transition starts a fresh idle cycle. Re-enabling an
    // already enabled timer leaves the current countdown and request unchanged.
    void SetEnabled(bool enabled);
    void OnEnterSleepMode(std::function<void()> callback);
    void OnExitSleepMode(std::function<void()> callback);
    // Called at most once per idle cycle, on the esp_timer task. WakeUp(), a
    // disabled-to-enabled transition, or activity resetting the countdown before
    // sleep mode rearms the request. A callback that declines a request must own
    // any retry or explicitly start a new idle cycle; returning does not retry.
    void OnShutdownRequest(std::function<void()> callback);
    // Reset the idle countdown and shutdown request, and exit sleep mode if active.
    void WakeUp();

private:
    void PowerSaveCheck();

    esp_timer_handle_t power_save_timer_ = nullptr;
    bool enabled_ = false;
    bool in_sleep_mode_ = false;
    bool is_wake_word_running_ = false;
    bool shutdown_requested_ = false;
    int ticks_ = 0;
    int cpu_max_freq_;
    int seconds_to_sleep_;
    int seconds_to_shutdown_;

    std::function<void()> on_enter_sleep_mode_;
    std::function<void()> on_exit_sleep_mode_;
    std::function<void()> on_shutdown_request_;
};
