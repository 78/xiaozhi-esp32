//--------------------------------------------------------------
//-- Oscillator.pde
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- (c) txp666 for esp32, 202503
//-- GPL license
//--------------------------------------------------------------
#include "oscillator.h"

#include <driver/ledc.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>

static const char *TAG = "Oscillator";

extern unsigned long IRAM_ATTR millis();

static ledc_channel_t next_free_channel = LEDC_CHANNEL_0;

Oscillator::Oscillator(int trim)
{
    trim_ = trim;
    diff_limit_ = 0;
    is_attached_ = false;

    sampling_period_ = 30;
    period_ = 2000;
    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;

    amplitude_ = 45;
    phase_ = 0;
    phase0_ = 0;
    offset_ = 0;
    stop_ = false;
    rev_ = false;

    pos_ = 90;
    previous_millis_ = 0;
}

Oscillator::~Oscillator()
{
    Detach();
}

uint32_t Oscillator::AngleToCompare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
}

bool Oscillator::NextSample()
{
    current_millis_ = millis();

    if (current_millis_ - previous_millis_ > sampling_period_)
    {
        previous_millis_ = current_millis_;
        return true;
    }

    return false;
}

void Oscillator::Attach(int pin, int channel, bool rev)
{
    if (is_attached_)
    {
        Detach();
    }

    pin_ = pin;
    rev_ = rev;
    ledc_channel_ = (ledc_channel_t)channel;

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution = LEDC_TIMER_13_BIT,
                                      .timer_num = LEDC_TIMER_1,
                                      .freq_hz = 50,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {.gpio_num = pin_,
                                          .speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = ledc_channel_,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .timer_sel = LEDC_TIMER_1,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_speed_mode_ = LEDC_LOW_SPEED_MODE;

    Write(pos_);
    previous_servo_command_millis_ = millis();

    is_attached_ = true;
}

void Oscillator::Detach()
{
    if (!is_attached_)
        return;
    ledc_stop(ledc_speed_mode_, ledc_channel_, 0);
    is_attached_ = false;
}

void Oscillator::SetT(unsigned int period)
{
    period_ = period;
    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;
}

void Oscillator::SetPosition(int position)
{
    if (is_attached_)
    {
        Write(position);
    }
}

void Oscillator::Refresh()
{
    if (is_attached_)
    {
        if (NextSample())
        {
            if (!stop_)
            {
                phase_ = phase_ + inc_;
                pos_ = round(amplitude_ * sin(phase_ + phase0_) + offset_);
                if (rev_)
                    pos_ = -pos_;
            }
            Write(pos_);
        }
    }
}

void Oscillator::Write(int position)
{
    if (diff_limit_ > 0)
    {
        long now = millis();
        float dt = (now - previous_servo_command_millis_) / 1000.0;
        if (dt > 0)
        {
            int max_diff = (int)(diff_limit_ * dt);
            int diff = position - pos_;
            if (abs(diff) > max_diff)
            {
                if (diff > 0)
                    position = pos_ + max_diff;
                else
                    position = pos_ - max_diff;
            }
        }
        previous_servo_command_millis_ = now;
    }

    pos_ = position;
    position = position + trim_;
    position = std::max(SERVO_MIN_DEGREE, std::min(position, SERVO_MAX_DEGREE));

    uint32_t pulse_width_us = AngleToCompare(position);
    uint32_t duty = (pulse_width_us * 8192) / 20000;
    ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty);
    ledc_update_duty(ledc_speed_mode_, ledc_channel_);
}
