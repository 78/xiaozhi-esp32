#include "pwm_led_controller.h"
#include <esp_err.h>

PwmLedController::PwmLedController(GpioLed* led) : led_(led) {
     // 设置默认亮度
     if (led_) {
        led_->SetBrightness(50);  // 默认 50% 亮度
        last_brightness_.store(50);
    }
    
    esp_timer_create_args_t args = {};
    args.callback = &PwmLedController::BlinkTimerCallback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "pwm_led_ctrl_timer";
    args.skip_unhandled_events = false;
    esp_timer_create(&args, &blink_timer_);
}

PwmLedController::~PwmLedController() {
    StopBlink();
    if (blink_timer_) {
        esp_timer_stop(blink_timer_);
        esp_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
}

bool PwmLedController::IsReady() const {
    return led_ != nullptr;
}

void PwmLedController::SetBrightnessPercent(uint8_t percent) {
    if (!IsReady()) return;
    if (percent > 100) percent = 100;
    last_brightness_.store(percent);
    led_->SetBrightness(percent);
    led_->TurnOn();  // TurnOn() 会将 duty_ 值写入硬件
}

void PwmLedController::TurnOn() {
    if (!IsReady()) return;
    // 保证占空比已按百分比设置
    led_->TurnOn();
}

void PwmLedController::TurnOff() {
    if (!IsReady()) return;
    led_->TurnOff();
}

void PwmLedController::BlinkOnce(int interval_ms) {
    if (!IsReady() || blink_timer_ == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex_);
    // 两个相位（on/off），总计 2 次
    blink_counter_.store(2);
    interval_ms_.store(interval_ms);
    on_phase_.store(true);
    blinking_.store(true);
    esp_timer_stop(blink_timer_);
    esp_timer_start_periodic(blink_timer_, interval_ms * 1000);
}

void PwmLedController::StartContinuousBlink(int interval_ms) {
    if (!IsReady() || blink_timer_ == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex_);
    blink_counter_.store(-1); // -1 表示无限
    interval_ms_.store(interval_ms);
    on_phase_.store(true);
    blinking_.store(true);
    esp_timer_stop(blink_timer_);
    esp_timer_start_periodic(blink_timer_, interval_ms * 1000);
}

void PwmLedController::StopBlink() {
    if (!IsReady() || blink_timer_ == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex_);
    blinking_.store(false);
    esp_timer_stop(blink_timer_);
}

uint8_t PwmLedController::LastBrightnessPercent() const {
    return last_brightness_.load();
}

void PwmLedController::BlinkTimerCallback(void* arg) {
    auto* self = static_cast<PwmLedController*>(arg);
    self->HandleBlinkTick();
}

void PwmLedController::HandleBlinkTick() {
    if (!IsReady()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (!blinking_.load()) {
        esp_timer_stop(blink_timer_);
        return;
    }

    // 交替 on/off
    if (on_phase_.load()) {
        led_->TurnOn();
    } else {
        led_->TurnOff();
    }
    on_phase_.store(!on_phase_.load());

    // 非无限闪烁则计数递减
    if (blink_counter_.load() > 0) {
        blink_counter_.store(blink_counter_.load() - 1);
        if (blink_counter_.load() == 0) {
            blinking_.store(false);
            esp_timer_stop(blink_timer_);
        }
    }
}