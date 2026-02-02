//--------------------------------------------------------------
//-- Oscillator.pde
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- (c) txp666 for esp32, 202503
//-- GPL license
//--------------------------------------------------------------
#include "sdkconfig.h"
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

    sampling_period_ = 20; // 20ms (50Hz) to match Servo PWM for smoother updates
    period_ = 2000;
    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;

    amplitude_ = 45;
    phase_ = 0;
    phase0_ = 0;
    offset_ = 0;
    stop_ = false;
    rev_ = false;

    pos_ = 0.0f; // Default to 0 (Center)
    last_pos_ = 0.0f; // Initialize tracking
    last_written_pos_ = 0.0f; // Initialize estimated physical position
    previous_millis_ = 0;
}

Oscillator::~Oscillator()
{
    Detach();
}

uint32_t Oscillator::AngleToCompare(int angle)
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_POS
    // Logic for MG90S 360 Degree (Positional / Winch)
    // Assumes 500us..2500us maps to -180..180 degrees (360 range)
    // Target 'angle' is in -90..90 range (from gait engine).
    // We map -90..90 input to physical angle, then to PWM.
    // If input 0 -> 1500us.
    // Input 90 -> 1500 + 90 * (2000/360) = 2000.
    return (uint32_t)(1500 + (angle * (2000.0 / 360.0)));
#else
    // Logic for SG90 180 Degree (Standard)
    // Assumes 500us..2500us maps to -90..90 degrees (180 range)
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
#endif
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

void Oscillator::SetPosition(float position)
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
                // DIRECT FLOAT ASSIGNMENT - NO ROUNDING
                // This preserves micro-movements for velocity calculation
                pos_ = amplitude_ * sin(phase_ + phase0_) + offset_;
                if (rev_)
                    pos_ = -pos_;
            }
            Write(pos_);
        }
    }
}

void Oscillator::Write(float position)
{
    if (diff_limit_ > 0)
    {
        long now = millis();
        float dt = (now - previous_servo_command_millis_) / 1000.0f;
        if (dt > 0)
        {
            float max_diff = diff_limit_ * dt;
            float diff = position - pos_;
            if (std::abs(diff) > max_diff)
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
    // Apply trim effectively? For positional 180, trim is usually angle.
    // For 360, we handle trim in the pulse calculation.
    // Let's keep position clean as "Angle" for calculation.
    float target_pos = position; 
    
    // Clamp for safety (logical angles)
    target_pos = std::max((float)SERVO_MIN_DEGREE, std::min(target_pos, (float)SERVO_MAX_DEGREE));

    uint32_t pulse_width_us = 0;

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // -- Continuous Rotation: Velocity Feedforward Control --
    
    long now = millis();
    float dt = (now - previous_servo_command_millis_) / 1000.0f;
    
    // Safety
    if (dt > 0.5f) dt = 0.03f; 
    if (dt < 0.001f) dt = 0.001f; 

    // Velocity = Change in Position / Time
    // FLOAT PRECISION: Now we can see 0.5 deg change!
    float velocity_deg_s = (position - last_pos_) / dt;
    
    // SAFETY CLAMP
    if (velocity_deg_s > 250.0f) velocity_deg_s = 250.0f;
    if (velocity_deg_s < -250.0f) velocity_deg_s = -250.0f;
    
    // Deadband Compensation
    const int DEADBAND_US = 50; 
    const int MAX_PWM_OFFSET = 500; 
    
    // Default Gain
    float k_gain = 20.0f;
#ifdef CONFIG_PUPPY_SERVO_CONTINUOUS_GAIN
    k_gain = CONFIG_PUPPY_SERVO_CONTINUOUS_GAIN;
#endif

    const float Kf = k_gain / 14.0f;
    
    int pwm_offset = 0;
    
    // Use a small epsilon for float comparison instead of > 0
    if (std::abs(position - last_pos_) > 0.001f) {
        // Calculate raw offset
        int raw_offset = (int)(std::abs(velocity_deg_s) * Kf);
        
        // Clamp
        if (raw_offset > (MAX_PWM_OFFSET - DEADBAND_US)) {
            raw_offset = MAX_PWM_OFFSET - DEADBAND_US;
        }
        
        // Apply deadband if significant movement requested
        // If velocity is extremely low (e.g. < 5 deg/s), maybe don't move? 
        // No, we want to creep. Let deadband handle it.
        
        int total_offset = DEADBAND_US + raw_offset;
        pwm_offset = (velocity_deg_s > 0) ? total_offset : -total_offset;
    }
    
    // Apply Trim as Center Pulse Offset
    pulse_width_us = 1500 + trim_ + pwm_offset;
    
    // Update state
    last_pos_ = position;
    previous_servo_command_millis_ = now;
    
    pos_ = position;

#else
    // -- Standard Positional Logic --
    last_pos_ = position; 
    // Convert float position to int angle for standard logic if needed, or update AngleToCompare
    pulse_width_us = AngleToCompare((int)target_pos); 
#endif

    uint32_t duty = (pulse_width_us * 8192) / 20000;
    ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty);
    ledc_update_duty(ledc_speed_mode_, ledc_channel_);
}

void Oscillator::Neutral()
{
    if (!is_attached_) return;

    // For Continuous: 1500us (+trim) is absolute stop.
    // For Positional: 1500us (+trim) is 0 degrees (Center).
    // Note: Ideally positional trim is an angle offset, but here we treat it as Pulse offset commonly.
    
    // Sync all position trackers to ensure delta is 0 for next movement
    last_pos_ = pos_;
    last_written_pos_ = pos_;

    uint32_t pulse_width_us = 1500 + trim_;
    uint32_t duty = (pulse_width_us * 8192) / 20000;
    ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty);
    ledc_update_duty(ledc_speed_mode_, ledc_channel_);
}

void Oscillator::SetSpeed(float speed)
{
    // Clamp speed -1.0 to 1.0
    if (speed > 1.0f) speed = 1.0f;
    if (speed < -1.0f) speed = -1.0f;
    
    // Deadband Compensation
    const int DEADBAND_US = 50;
    // Remaining range after deadband
    int pwm_range = 1000 - DEADBAND_US; 
    
    int pwm_target = 1500;
    
    if (abs(speed) > 0.001f) {
        int offset = DEADBAND_US + (int)(abs(speed) * pwm_range);
        if (speed > 0) pwm_target = 1500 + offset;
        else pwm_target = 1500 - offset;
    }

    // Apply Trim
    pwm_target += trim_;

    uint32_t duty = (pwm_target * 8192) / 20000;
    ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty);
    ledc_update_duty(ledc_speed_mode_, ledc_channel_);
}
