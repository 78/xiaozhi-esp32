#include "pwm_led_controller.h"
#include <esp_err.h>

PwmLedController::PwmLedController(GpioLed* led) : led_(led) {
     // 设置默认亮度
     if (led_) {
        led_->SetBrightness(100);  // 默认 50% 亮度
        last_brightness_.store(100);
    }
    
    esp_timer_create_args_t args = {};
    args.callback = &PwmLedController::BlinkTimerCallback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "pwm_led_ctrl_timer";
    args.skip_unhandled_events = false;
    esp_timer_create(&args, &blink_timer_);

    esp_timer_create_args_t bargs = {};
    bargs.callback = &PwmLedController::BreathTimerCallback;
    bargs.arg = this;
    bargs.dispatch_method = ESP_TIMER_TASK;
    bargs.name = "pwm_led_breath_timer";
    bargs.skip_unhandled_events = false;
    esp_timer_create(&bargs, &breath_timer_);
}

PwmLedController::~PwmLedController() {
    StopBlink();
    StopBreathing();
    if (blink_timer_) {
        esp_timer_stop(blink_timer_);
        esp_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
    if (breath_timer_) {
        esp_timer_stop(breath_timer_);
        esp_timer_delete(breath_timer_);
        breath_timer_ = nullptr;
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

void PwmLedController::StartBreathing(int interval_ms, uint8_t min_percent, uint8_t max_percent) {
    if (!IsReady() || breath_timer_ == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    blinking_.store(false);
    if (min_percent > max_percent) {
        uint8_t t = min_percent;
        min_percent = max_percent;
        max_percent = t;
    }
    if (max_percent > 100) max_percent = 100;
    breath_interval_ms_.store(interval_ms);
    breath_min_.store(min_percent);
    breath_max_.store(max_percent);
    breath_current_.store(min_percent);
    breath_up_.store(true);
    breathing_.store(true);
    SetBrightnessPercent(min_percent);
    esp_timer_start_periodic(breath_timer_, interval_ms * 1000);
}

void PwmLedController::StopBreathing() {
    if (!IsReady() || breath_timer_ == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex_);
    breathing_.store(false);
    esp_timer_stop(breath_timer_);
}

bool PwmLedController::IsBreathing() const {
    return breathing_.load();
}

void PwmLedController::BreathTimerCallback(void* arg) {
    auto* self = static_cast<PwmLedController*>(arg);
    self->HandleBreathTick();
}

void PwmLedController::HandleBreathTick() {
    if (!IsReady()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (!breathing_.load()) {
        esp_timer_stop(breath_timer_);
        return;
    }

    uint8_t cur = breath_current_.load();
    uint8_t minp = breath_min_.load();
    uint8_t maxp = breath_max_.load();
    bool up = breath_up_.load();
    int step = 2;
    if (up) {
        if (cur + step >= maxp) {
            cur = maxp;
            breath_up_.store(false);
        } else {
            cur = cur + step;
        }
    } else {
        if (cur <= minp + step) {
            cur = minp;
            breath_up_.store(true);
        } else {
            cur = cur - step;
        }
    }
    breath_current_.store(cur);
    SetBrightnessPercent(cur);
}
