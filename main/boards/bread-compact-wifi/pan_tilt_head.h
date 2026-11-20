#ifndef PAN_TILT_HEAD_H
#define PAN_TILT_HEAD_H

#include <atomic>
#include <mutex>

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "device_state.h"

// PWM driver for Tower Pro SG90 9g analog micro servos (pan + tilt).

class PanTiltHead {
public:
    PanTiltHead() = default;
    ~PanTiltHead();

    PanTiltHead(const PanTiltHead&) = delete;
    PanTiltHead& operator=(const PanTiltHead&) = delete;

    void Initialize();
    void Start();

    void SetPanAngle(int angle);
    void SetTiltAngle(int angle);
    void SetPose(int pan, int tilt);
    void LookHome();

    int pan_angle() const { return static_cast<int>(current_pan_ + 0.5f); }
    int tilt_angle() const { return static_cast<int>(current_tilt_ + 0.5f); }

    void RegisterMcpTools();

private:
    enum class MotionMode {
        Home,
        IdleWander,
        Thinking,
        Listening,
        Speaking,
        Manual,
    };

    void SetupChannel(ledc_channel_t channel, gpio_num_t gpio);
    void WriteAngle(ledc_channel_t channel, int angle);
    int ClampPan(int angle) const;
    int ClampTilt(int angle) const;
    void ApplyTargetsUnlocked();
    void UpdateFromDeviceState(DeviceState state);
    void TickAnimation();
    static void MotionTaskTrampoline(void* arg);
    void MotionTask();

    std::mutex mutex_;
    TaskHandle_t motion_task_ = nullptr;
    bool initialized_ = false;

    float current_pan_ = SERVO_PAN_HOME;
    float current_tilt_ = SERVO_TILT_HOME;
    float target_pan_ = SERVO_PAN_HOME;
    float target_tilt_ = SERVO_TILT_HOME;

    MotionMode mode_ = MotionMode::Home;
    DeviceState last_state_ = kDeviceStateUnknown;
    int64_t anim_start_us_ = 0;
    int64_t manual_until_us_ = 0;
};

#endif  // PAN_TILT_HEAD_H
