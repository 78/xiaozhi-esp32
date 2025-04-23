#include "oscillator.h"

#include <driver/ledc.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>

static const char* TAG = "oscillator";

extern unsigned long IRAM_ATTR millis();

static ledc_channel_t next_free_channel = LEDC_CHANNEL_0;

Oscillator::Oscillator(int trim) {
    _trim = trim;
    _diff_limit = 0;
    _is_attached = false;

    _samplingPeriod = 30;
    _period = 2000;
    _numberSamples = _period / _samplingPeriod;
    _inc = 2 * M_PI / _numberSamples;

    _amplitude = 45;
    _phase = 0;
    _phase0 = 0;
    _offset = 0;
    _stop = false;
    _rev = false;

    _pos = 90;
    _previousMillis = 0;
}

Oscillator::~Oscillator() {
    detach();
}

uint32_t Oscillator::angle_to_compare(int angle) {
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
}

bool Oscillator::next_sample() {
    _currentMillis = millis();

    if (_currentMillis - _previousMillis > _samplingPeriod) {
        _previousMillis = _currentMillis;
        return true;
    }

    return false;
}

void Oscillator::attach(int pin, bool rev) {
    if (_is_attached) {
        detach();
    }

    _pin = pin;
    _rev = rev;

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution = LEDC_TIMER_13_BIT,
                                      .timer_num = LEDC_TIMER_1,
                                      .freq_hz = 50,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    static int last_channel = 0;
    last_channel = (last_channel + 1) % 7 + 1;
    _ledc_channel = (ledc_channel_t)last_channel;

    ledc_channel_config_t ledc_channel = {.gpio_num = _pin,
                                          .speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = _ledc_channel,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .timer_sel = LEDC_TIMER_1,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    _ledc_speed_mode = LEDC_LOW_SPEED_MODE;

    _pos = 90;
    write(_pos);
    _previousServoCommandMillis = millis();

    _is_attached = true;
}

void Oscillator::detach() {
    if (!_is_attached)
        return;

    ESP_ERROR_CHECK(ledc_stop(_ledc_speed_mode, _ledc_channel, 0));

    _is_attached = false;
}

void Oscillator::SetT(unsigned int T) {
    _period = T;

    _numberSamples = _period / _samplingPeriod;
    _inc = 2 * M_PI / _numberSamples;
}

void Oscillator::SetPosition(int position) {
    write(position);
}

void Oscillator::refresh() {
    if (next_sample()) {
        if (!_stop) {
            int pos = std::round(_amplitude * std::sin(_phase + _phase0) + _offset);
            if (_rev)
                pos = -pos;
            write(pos + 90);
        }

        _phase = _phase + _inc;
    }
}

void Oscillator::write(int position) {
    if (!_is_attached)
        return;

    long currentMillis = millis();
    if (_diff_limit > 0) {
        int limit = std::max(
            1, (((int)(currentMillis - _previousServoCommandMillis)) * _diff_limit) / 1000);
        if (abs(position - _pos) > limit) {
            _pos += position < _pos ? -limit : limit;
        } else {
            _pos = position;
        }
    } else {
        _pos = position;
    }
    _previousServoCommandMillis = currentMillis;

    int angle = _pos + _trim;

    angle = std::min(std::max(angle, 0), 180);

    uint32_t duty = (uint32_t)(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);

    ESP_ERROR_CHECK(ledc_set_duty(_ledc_speed_mode, _ledc_channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(_ledc_speed_mode, _ledc_channel));
}
