#include "pan_tilt_head.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>

#include <esp_log.h>
#include <esp_timer.h>

#include "application.h"
#include "mcp_server.h"

#define TAG "PanTiltHead"

namespace {
constexpr uint32_t kDutyResolutionBits = 13;
constexpr uint32_t kMaxDuty = (1u << kDutyResolutionBits) - 1u;
constexpr uint32_t kPeriodUs = 20000;
constexpr int kMotionPeriodMs = 20;
// SG90 is ~0.1 s / 60°; ~3° per 20 ms keeps plastic gears from slamming.
constexpr int64_t kManualHoldUs = 8 * 1000 * 1000;
}

PanTiltHead::~PanTiltHead() {
    if (motion_task_ != nullptr) {
        vTaskDelete(motion_task_);
        motion_task_ = nullptr;
    }
}

void PanTiltHead::Initialize() {
    if (initialized_) {
        return;
    }

    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_13_BIT;
    timer.timer_num = SERVO_LEDC_TIMER;
    timer.freq_hz = SERVO_PWM_FREQ_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    SetupChannel(SERVO_PAN_LEDC_CHANNEL, SERVO_PAN_GPIO);
    SetupChannel(SERVO_TILT_LEDC_CHANNEL, SERVO_TILT_GPIO);

    initialized_ = true;
    LookHome();
    ESP_LOGI(TAG, "SG90 9g ready pan=GPIO%d tilt=GPIO%d (1000-2000us @ 50Hz)",
             SERVO_PAN_GPIO, SERVO_TILT_GPIO);
}

void PanTiltHead::Start() {
    if (motion_task_ != nullptr) {
        return;
    }
    xTaskCreate(MotionTaskTrampoline, "pan_tilt", 4096, this, 3, &motion_task_);
}

void PanTiltHead::SetupChannel(ledc_channel_t channel, gpio_num_t gpio) {
    ledc_channel_config_t cfg = {};
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.channel = channel;
    cfg.timer_sel = SERVO_LEDC_TIMER;
    cfg.intr_type = LEDC_INTR_DISABLE;
    cfg.gpio_num = gpio;
    cfg.duty = 0;
    cfg.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&cfg));
}

void PanTiltHead::WriteAngle(ledc_channel_t channel, int angle) {
    angle = std::max(0, std::min(180, angle));
    uint32_t pulse_us = SERVO_MIN_PULSE_US +
                        static_cast<uint32_t>(angle) * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / 180;
    uint32_t duty = pulse_us * kMaxDuty / kPeriodUs;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

int PanTiltHead::ClampPan(int angle) const {
    return std::max(SERVO_PAN_MIN, std::min(SERVO_PAN_MAX, angle));
}

int PanTiltHead::ClampTilt(int angle) const {
    return std::max(SERVO_TILT_MIN, std::min(SERVO_TILT_MAX, angle));
}

void PanTiltHead::ApplyTargetsUnlocked() {
    if (!initialized_) {
        return;
    }
    WriteAngle(SERVO_PAN_LEDC_CHANNEL, static_cast<int>(current_pan_ + 0.5f));
    WriteAngle(SERVO_TILT_LEDC_CHANNEL, static_cast<int>(current_tilt_ + 0.5f));
}

void PanTiltHead::SetPanAngle(int angle) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = MotionMode::Manual;
    manual_until_us_ = esp_timer_get_time() + kManualHoldUs;
    target_pan_ = static_cast<float>(ClampPan(angle));
}

void PanTiltHead::SetTiltAngle(int angle) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = MotionMode::Manual;
    manual_until_us_ = esp_timer_get_time() + kManualHoldUs;
    target_tilt_ = static_cast<float>(ClampTilt(angle));
}

void PanTiltHead::SetPose(int pan, int tilt) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = MotionMode::Manual;
    manual_until_us_ = esp_timer_get_time() + kManualHoldUs;
    target_pan_ = static_cast<float>(ClampPan(pan));
    target_tilt_ = static_cast<float>(ClampTilt(tilt));
}

void PanTiltHead::LookHome() {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = MotionMode::Manual;
    manual_until_us_ = esp_timer_get_time() + kManualHoldUs;
    target_pan_ = SERVO_PAN_HOME;
    target_tilt_ = SERVO_TILT_HOME;
    current_pan_ = target_pan_;
    current_tilt_ = target_tilt_;
    ApplyTargetsUnlocked();
}

void PanTiltHead::UpdateFromDeviceState(DeviceState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t now = esp_timer_get_time();
    const bool state_changed = (state != last_state_);
    last_state_ = state;

    if (mode_ == MotionMode::Manual && now < manual_until_us_ && !state_changed) {
        return;
    }

    if (state_changed) {
        anim_start_us_ = now;
    }

    switch (state) {
        case kDeviceStateIdle:
            mode_ = MotionMode::IdleWander;
            break;
        case kDeviceStateConnecting:
            mode_ = MotionMode::Thinking;
            break;
        case kDeviceStateListening:
            mode_ = MotionMode::Listening;
            break;
        case kDeviceStateSpeaking:
            mode_ = MotionMode::Speaking;
            break;
        default:
            mode_ = MotionMode::Home;
            target_pan_ = SERVO_PAN_HOME;
            target_tilt_ = SERVO_TILT_HOME;
            break;
    }
}

void PanTiltHead::TickAnimation() {
    std::lock_guard<std::mutex> lock(mutex_);
    const float t = (esp_timer_get_time() - anim_start_us_) / 1e6f;

    switch (mode_) {
        case MotionMode::IdleWander:
            target_pan_ = SERVO_PAN_HOME + 18.0f * std::sin(t * 0.35f);
            target_tilt_ = SERVO_TILT_HOME + 6.0f * std::sin(t * 0.22f + 1.2f);
            break;
        case MotionMode::Thinking:
            target_pan_ = SERVO_PAN_HOME + 28.0f * std::sin(t * 1.6f);
            target_tilt_ = SERVO_TILT_HOME + 8.0f * std::sin(t * 0.9f);
            break;
        case MotionMode::Listening:
            target_pan_ = SERVO_PAN_HOME + 8.0f * std::sin(t * 0.5f);
            target_tilt_ = SERVO_TILT_HOME + 12.0f;
            break;
        case MotionMode::Speaking:
            target_pan_ = SERVO_PAN_HOME;
            target_tilt_ = SERVO_TILT_HOME + 16.0f * std::sin(t * 5.0f);
            break;
        case MotionMode::Home:
            target_pan_ = SERVO_PAN_HOME;
            target_tilt_ = SERVO_TILT_HOME;
            break;
        case MotionMode::Manual:
            break;
    }

    target_pan_ = static_cast<float>(ClampPan(static_cast<int>(target_pan_)));
    target_tilt_ = static_cast<float>(ClampTilt(static_cast<int>(target_tilt_)));

    auto slew = [](float current, float target) {
        float delta = target - current;
        if (delta > kSlewPerTick) {
            return current + kSlewPerTick;
        }
        if (delta < -kSlewPerTick) {
            return current - kSlewPerTick;
        }
        return target;
    };
    current_pan_ = slew(current_pan_, target_pan_);
    current_tilt_ = slew(current_tilt_, target_tilt_);
    ApplyTargetsUnlocked();
}

void PanTiltHead::MotionTaskTrampoline(void* arg) {
    static_cast<PanTiltHead*>(arg)->MotionTask();
}

void PanTiltHead::MotionTask() {
    while (true) {
        UpdateFromDeviceState(Application::GetInstance().GetDeviceState());
        TickAnimation();
        vTaskDelay(pdMS_TO_TICKS(kMotionPeriodMs));
    }
}

void PanTiltHead::RegisterMcpTools() {
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool("self.head.move",
        "Move the robot head. pan is left/right degrees (30-150, 90=center, smaller=left). "
        "tilt is up/down degrees (45-135, 90=level, larger=look up). "
        "Use when the user asks the robot to look around, face a direction, nod, or pose.",
        PropertyList({
            Property("pan", kPropertyTypeInteger, SERVO_PAN_HOME, SERVO_PAN_MIN, SERVO_PAN_MAX),
            Property("tilt", kPropertyTypeInteger, SERVO_TILT_HOME, SERVO_TILT_MIN, SERVO_TILT_MAX)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            SetPose(properties["pan"].value<int>(), properties["tilt"].value<int>());
            return true;
        });

    mcp.AddTool("self.head.home",
        "Return the robot head to the rest/center position.",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            LookHome();
            return true;
        });

    mcp.AddTool("self.head.get_pose",
        "Get the current head pan and tilt angles in degrees.",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            char json[64];
            snprintf(json, sizeof(json), "{\"pan\":%d,\"tilt\":%d}", pan_angle(), tilt_angle());
            return std::string(json);
        });
}
