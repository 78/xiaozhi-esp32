#include "puppy_movements.h"
#include <algorithm>
#include <cmath>
#include "freertos/idf_additions.h"
#include "sdkconfig.h"

static const char *TAG = "PuppyMovements";

Puppy::Puppy()
{
    is_puppy_resting_ = false;
    // Initialize all servo pins to -1 (not connected)
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
        servo_speed_scale_[i] = 1.0f; // Default 1.0
        estimated_angle_[i] = 0.0f; // Assume start at Sit (0 degrees)
    }
}

Puppy::~Puppy()
{
    DetachServos();
}

unsigned long IRAM_ATTR millis()
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void Puppy::Init(int fl_leg, int fr_leg, int bl_leg, int br_leg, int tail)
{
    servo_pins_[FL_LEG] = fl_leg;
    servo_pins_[FR_LEG] = fr_leg;
    servo_pins_[BL_LEG] = bl_leg;
    servo_pins_[BR_LEG] = br_leg;
    servo_pins_[TAIL] = tail;

    AttachServos();
    is_puppy_resting_ = false;
    
    // Enable servo speed limiting by default for smoother movement
    // 60 degrees/sec is a gentle speed.
    EnableServoLimit(60); 
}

void Puppy::AttachServos()
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            // Use fixed channels starting from 2 to avoid conflicts with Backlight (usually Ch 0)
            // FL=2, FR=3, BL=4, BR=5, Tail=6
            servo_[i].Attach(servo_pins_[i], i + 2);
        }
    }
}

void Puppy::DetachServos()
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].Detach();
        }
    }
}

void Puppy::SetTrims(int fl_leg, int fr_leg, int bl_leg, int br_leg, int tail)
{
    servo_trim_[FL_LEG] = fl_leg;
    servo_trim_[FR_LEG] = fr_leg;
    servo_trim_[BL_LEG] = bl_leg;
    servo_trim_[BR_LEG] = br_leg;
    servo_trim_[TAIL] = tail;

    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].SetTrim(servo_trim_[i]);
            // Force update PWM signal immediately for feedback
            if (!is_puppy_resting_) {
                 servo_[i].Neutral();
            }
        }
    }
}

void Puppy::SetSpeedScales(float fl, float fr, float bl, float br, float tail)
{
    servo_speed_scale_[FL_LEG] = fl;
    servo_speed_scale_[FR_LEG] = fr;
    servo_speed_scale_[BL_LEG] = bl;
    servo_speed_scale_[BR_LEG] = br;
    servo_speed_scale_[TAIL] = tail;
    ESP_LOGI(TAG, "Speed Scales Set: %.2f %.2f %.2f %.2f %.2f", fl, fr, bl, br, tail);
}

void Puppy::MoveServos(int time, int servo_target[])
{
    if (GetRestState() == true)
    {
        SetRestState(false);
    }


    final_time_ = millis() + time;
    if (time > 10)
    {
        // Capture start positions
        int start_pos[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) start_pos[i] = servo_[i].GetPosition();
            else start_pos[i] = 0;
        }

        int steps = time / 10;
        for (int s = 1; s <= steps; s++)
        {
            float progress = (float)s / (float)steps;
            for (int i = 0; i < SERVO_COUNT; i++)
            {
                if (servo_pins_[i] != -1)
                {
                    // Linear Interpolation: Start + (Diff * Progress)
                    int new_pos = start_pos[i] + (int)((servo_target[i] - start_pos[i]) * progress);
                    servo_[i].SetPosition(new_pos);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    else
    {
        for (int i = 0; i < SERVO_COUNT; i++)
        {
            if (servo_pins_[i] != -1)
            {
                servo_[i].SetPosition(servo_target[i]);
            }
        }
    }
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].SetPosition(servo_target[i]);
        }
    }


    // For Continuous servos ONLY: Stop motors after move completes
    // 360_POS and 180 are positional - they hold angle automatically
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT)
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
             servo_[i].Neutral();
        }
    }
#endif

   
    // Update estimated angle for ALL servo types (180 and 360)
    // This allows GentleStand to know the current pose.
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            estimated_angle_[i] = (float)servo_target[i];
        }
    }
    ESP_LOGD(TAG, "MoveServos: Synced estimated_angle_ to targets");
}


void Puppy::MoveServosVelocity(int time, float servo_velocity[])
{
    if (GetRestState() == true)
    {
        SetRestState(false);
    }
    
    // Set speed for all servos
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].SetSpeed(servo_velocity[i]);
        }
    }

    // Wait for the duration
    if (time > 0)
    {
        unsigned long start_time = millis();
        while((millis() - start_time) < time) {
             vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Stop after duration
        for (int i = 0; i < SERVO_COUNT; i++)
        {
            if (servo_pins_[i] != -1)
            {
                servo_[i].Neutral(); // Stop
            }
        }
    }
}

// -- SAFE ANGLE LIMITS (-90 to +90 corresponds to 0-180 degree range) --
#define SERVO_SAFE_MIN -90
#define SERVO_SAFE_MAX 90

// 360 Servo: Move Relative Time-Based
void Puppy::MoveRelative(int relative_angle, int speed_deg_per_sec)
{
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    if (speed_deg_per_sec <= 0) speed_deg_per_sec = 60;

    int valid_servos = 0;
    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1 && i != TAIL) valid_servos++;
    }
    if(valid_servos == 0) return;

    // --- CLAMPING LOGIC START ---
    // We must ensure the movement doesn't push ANY leg out of bounds.
    // Since this moves ALL legs by the same amount, we check the leg that is closest to the limit.
    
    // Find effective relative angle that keeps ALL legs within bounds
    // But realistically, estimated_angle_ should be similar for all legs in this gait unless drifted.
    // Let's protect each leg individually if possible, but MoveServosVelocity takes ONE duration.
    // So we clamp the relative_angle based on the "worst case" leg.
    
    int clamped_relative = relative_angle;
    
    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1 || i == TAIL) continue;
        
        float current = estimated_angle_[i];
        float potential_target = current + relative_angle;
        
        if (potential_target > SERVO_SAFE_MAX) {
            int max_allowable = (int)(SERVO_SAFE_MAX - current);
            if (max_allowable < clamped_relative) clamped_relative = max_allowable;
        }
        if (potential_target < SERVO_SAFE_MIN) {
            int max_allowable = (int)(SERVO_SAFE_MIN - current);
            if (max_allowable > clamped_relative) clamped_relative = max_allowable;
        }
    }
    
    if (abs(clamped_relative) < 1) {
        ESP_LOGW(TAG, "MoveRelative: Movement blocked by limits (Req=%d)", relative_angle);
        return; 
    }
    
    // --- CLAMPING LOGIC END ---

    // Calculate Duration using CLAMPED angle
    float duration_ms = (abs(clamped_relative) / (float)speed_deg_per_sec) * 1000.0f;
    
    float velocities[SERVO_COUNT] = {0};
    
    float direction = (clamped_relative > 0) ? 1.0f : -1.0f;
    float pwm_speed = ((float)speed_deg_per_sec / 360.0f) * direction;
    
    // Calculate velocities for each servo
    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1 || i == TAIL) continue;
        
        float v = pwm_speed;
        
        // Apply Per-Motor Speed Scale
        v *= servo_speed_scale_[i];
        
        if (i == FR_LEG || i == BR_LEG) {
             v = -v; // Invert Right Side
        }
        velocities[i] = v;
        
        // Update estimated angle with CLAMPED value
        estimated_angle_[i] += clamped_relative;
        
        // Double check clamping (floating point drift protection)
        if (estimated_angle_[i] > SERVO_SAFE_MAX) estimated_angle_[i] = SERVO_SAFE_MAX;
        if (estimated_angle_[i] < SERVO_SAFE_MIN) estimated_angle_[i] = SERVO_SAFE_MIN;
    }
    
    ESP_LOGI(TAG, "MoveRelative: Req=%d Eff=%d dur=%.0f", relative_angle, clamped_relative, duration_ms);
    // Execute
    MoveServosVelocity((int)duration_ms, velocities);
#endif

#if !defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) && !defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // Positional Mode Implementation
    int targets[SERVO_COUNT];
    // Calculate duration based on speed (default 60 deg/sec)
    if (speed_deg_per_sec <= 0) speed_deg_per_sec = 60;
    int duration = (abs(relative_angle) * 1000) / speed_deg_per_sec;

    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1 && i != TAIL) {
             // Get current target position from oscillator
             int current = servo_[i].GetPosition();
             targets[i] = current + relative_angle;
        } else {
             if (servo_pins_[i] != -1) targets[i] = servo_[i].GetPosition();
             else targets[i] = 0;
        }
    }
    MoveServos(duration, targets);
#endif
}

void Puppy::MoveToAngle(int target_angle, int speed_deg_per_sec)
{
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // For 360 servos: Move ALL legs to the same target angle
    
    // --- CLAMPING LOGIC ---
    if (target_angle > SERVO_SAFE_MAX) target_angle = SERVO_SAFE_MAX;
    if (target_angle < SERVO_SAFE_MIN) target_angle = SERVO_SAFE_MIN;
    // ----------------------

    float velocities[SERVO_COUNT] = {0};
    float max_duration = 0;
    
    if (speed_deg_per_sec <= 0) speed_deg_per_sec = 30;

    for(int i = 0; i < SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1 || i == TAIL) continue;

        float current = estimated_angle_[i];
        
        // Self-correction: If current estimate is out of bounds, snap it?
        // Better: calculate delta from current estimate.
        
        float delta = target_angle - current;
        
        // Skip if already at target
        if(std::abs(delta) < 1.0f) continue;
        
        float duration_ms = (std::abs(delta) / (float)speed_deg_per_sec) * 1000.0f;
        if(duration_ms > max_duration) max_duration = duration_ms;
        
        // Direction
        float direction = (delta > 0) ? 1.0f : -1.0f;
        float pwm = ((float)speed_deg_per_sec / 360.0f) * direction;
        
        // Apply Speed Scale
        pwm *= servo_speed_scale_[i];
        
        velocities[i] = pwm;
        estimated_angle_[i] = target_angle;
    }
    
    if (max_duration > 0) {
        ESP_LOGI(TAG, "MoveToAngle: Target=%d Duration=%.0fms", target_angle, max_duration);
        MoveServosVelocity((int)max_duration, velocities);
    } else {
        ESP_LOGI(TAG, "MoveToAngle: Already at target %d", target_angle);
    }
#else
    // Fallback for Positional Servos
    int targets[SERVO_COUNT];
    for(int i=0; i<SERVO_COUNT; i++) targets[i] = target_angle;
    MoveServos(1000, targets);
#endif
}

void Puppy::MoveSingle(int position, int servo_number)
{
    if (position > 90) position = 90;
    if (position < -90) position = -90;

    if (GetRestState() == true) {
        SetRestState(false);
    }

    // Create target array with current positions
    int targets[SERVO_COUNT];
    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1) {
             // Keep other servos where they are
             targets[i] = servo_[i].GetPosition();
        } else {
             targets[i] = 0;
        }
    }
    
    // Update the specific servo target
    targets[servo_number] = position;
    
    // Move smoothly over 500ms
    // This interpolation is REQUIRED for Velocity Feedforward to work!
    MoveServos(500, targets); 
}

void Puppy::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                            double phase_diff[SERVO_COUNT], float cycle)
{
    ESP_LOGI(TAG, "OscillateServos Start: Period=%d Cycle=%.1f", period, cycle);
    if (GetRestState() == true)
    {
        SetRestState(false);
    }

    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_[i].SetO(offset[i]);
        servo_[i].SetA(amplitude[i]);
        servo_[i].SetT(period);
        servo_[i].SetPh(phase_diff[i]);
    }
    double ref = millis();
    double end_time = ref + period * cycle;
    while (millis() < end_time)
    {
        for (int i = 0; i < SERVO_COUNT; i++)
        {
            servo_[i].Refresh();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Instead of abrupt stop (Neutral), move back to Offset (Neutral Position)
    // This ensures the robot ends in the "Standing" or configured central pose.
    ESP_LOGI(TAG, "OscillateServos: Returning to Neutral/Offset");
    int targets[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            targets[i] = offset[i];
        }
        else
        {
            targets[i] = 0;
        }
    }
    
    // Move smoothly to neutral over 500ms
    // Use a fixed time or calculate based on distance? 
    // 500ms is usually good enough for small gait corrections.
    MoveServos(500, targets);

    // Update estimated angle to be safe
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if(servo_pins_[i] != -1) {
            estimated_angle_[i] = targets[i];
        }
    }
}

void Puppy::Home()
{
    if (is_puppy_resting_)
        return;

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Use velocity-based movement
    ESP_LOGI(TAG, "Home() [360] All legs to 0.");
    MoveToAngle(0, 60);
#else
    // 180: Direct positional control
    ESP_LOGI(TAG, "Home() [180] All servos to 0.");
    int homes[SERVO_COUNT] = {0, 0, 0, 0, 0};
    MoveServos(1000, homes);
#endif
    SetRestState(true);
}

bool Puppy::GetRestState()
{
    return is_puppy_resting_;
}

void Puppy::SetRestState(bool state)
{
    if (state != is_puppy_resting_)
    {
        is_puppy_resting_ = state;
        if (!is_puppy_resting_)
        {
            AttachServos();
        }
        else
        {
            DetachServos();
        }
    }
}

void Puppy::EnableServoLimit(int speed_limit_degree_per_sec)
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_[i].SetLimiter(speed_limit_degree_per_sec);
    }
}

void Puppy::DisableServoLimit()
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_[i].DisableLimiter();
    }
}

// --- High Level Movements ---



void Puppy::Walk(float steps, int period, int dir)
{
    // Enforce Standing Pose before starting gait
    GentleStand();

    // Both 180 and 360 servos use OscillateServos
    // For 360: Oscillator::Write converts position delta to velocity automatically
    
    // Creep Gait / Wave Gait for 1-DOF per leg
    // Sequence: FL -> BR -> FR -> BL (Wave Gait)
    // Phases (0-1): FL=0, BR=0.25, FR=0.5, BL=0.75
    
    // Trot Gait (Diagonal Pairs)
    // FL & BR move together (Phase 0)
    // FR & BL move together (Phase 180)
    // This provides a stable 2-point balanced gait.
    
    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0};
    int offset[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0};

    // Slow down period for stability if it's too fast
    if (period < 1000) period = 1000;

    if (dir == FORWARD)
    {
        phase_diff[FL_LEG] = DEG2RAD(0);
        phase_diff[BR_LEG] = DEG2RAD(0);    // Diagonal with FL
        
        phase_diff[FR_LEG] = DEG2RAD(180);
        phase_diff[BL_LEG] = DEG2RAD(180);  // Diagonal with FR
    }
    else // BACKWARD
    {
        // For backward, just shift by 180 (or invert amplitude, but phase shift is cleaner)
        phase_diff[FL_LEG] = DEG2RAD(180);
        phase_diff[BR_LEG] = DEG2RAD(180);
        
        phase_diff[FR_LEG] = DEG2RAD(0);
        phase_diff[BL_LEG] = DEG2RAD(0);
    }

    OscillateServos(amplitude, offset, period, phase_diff, steps);
}



void Puppy::Turn(float steps, int period, int dir)
{
    // Enforce Standing Pose before starting turn
    GentleStand();

    // Both 180 and 360 servos use OscillateServos
    // Turn by moving legs on opposite sides in opposite directions
    
    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0};
    int offset[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0};

    if (dir == LEFT)
    {
        // Turn Left (CCW): Diagonal pairs
        phase_diff[FL_LEG] = DEG2RAD(0);
        phase_diff[BR_LEG] = DEG2RAD(0); 
        phase_diff[FR_LEG] = DEG2RAD(180); 
        phase_diff[BL_LEG] = DEG2RAD(180); 
    }
    else // RIGHT
    {
        phase_diff[FL_LEG] = DEG2RAD(180);
        phase_diff[BR_LEG] = DEG2RAD(180);
        phase_diff[FR_LEG] = DEG2RAD(0);
        phase_diff[BL_LEG] = DEG2RAD(0);
    }

    OscillateServos(amplitude, offset, period, phase_diff, steps);
}


void Puppy::Sit()
{
    if (GetRestState() == true) {
        SetRestState(false);
    }

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Continuous rotation servos cannot "hold" a position like -60 degrees.
    // If we try to MoveToAngle(-60), they will just spin. 
    // The best way to "Sit" (or lower chassis) is to Relax/Detach the servos and let gravity work.
    // Or we could move to 0 (Stand/Home) if we want a known state.
    // Given the user report of "Spinning", we must STOP driving them.
    ESP_LOGI(TAG, "Sit() [360] Continuous Servos cannot hold pose. Relaxing motors.");
    
    // Option A: Detach (Limp) - True Sit
    DetachServos();
    
    // Option B: Stop (Neutral) - Hold current position (if friction allows)
    // for(int i=0; i<SERVO_COUNT; i++) servo_[i].Neutral();
#else
    // 180: Direct positional control
    ESP_LOGI(TAG, "Sit() [180] All legs to -60.");
    int sit_pos[SERVO_COUNT] = {-60, -60, -60, -60, 0}; 
    MoveServos(1000, sit_pos);
#endif
}



void Puppy::WagTail(int period, int amplitude)
{
    // -- IMPROVED "SMART" WAG -- 
    // Uses a Sine Window to ramp amplitude up and down.
    // This prevents the "Jerk" at start/stop and looks more organic.

    // 1. Snappier Speed: Cap period at 350ms to ensure it looks "Happy/Excited"
    if (period > 350) period = 350;

    // 2. Ensure enough cycles for the envelope to look good
    int cycles = 6; 
    
    // Total Duration
    unsigned long duration = cycles * period;
    unsigned long start_time = millis();
    
    // Ensure active
    if (GetRestState() == true) {
        SetRestState(false);
    }

    ESP_LOGI(TAG, "SmartWag: Period=%d Amp=%d Cycles=%d", period, amplitude, cycles);

    while (true)
    {
        unsigned long now = millis();
        unsigned long elapsed = now - start_time;
        
        if (elapsed > duration) break;

        // 3. Calculate Envelopes
        // Global Progress (0.0 to 1.0) for Amplitude Envelope
        float global_progress = (float)elapsed / (float)duration;
        
        // Amplitude Envelope: Sine Window (Starts at 0, Peals at 1, Ends at 0)
        // using sin(0..PI)
        float amp_scale = sin(global_progress * M_PI);
        
        // Current instantaneous amplitude
        float current_amp = amplitude * amp_scale;

        // 4. Calculate Waveform
        // Oscillator Phase (0..2PI per period)
        float cycle_progress = (float)(elapsed % period) / (float)period;
        float phase = cycle_progress * 2 * M_PI;
        
        // Position = Amp * sin(phase)
        int pos = (int)(current_amp * sin(phase));

        // 5. Write to Servo
        // We use MoveSingle logic directly to avoid overhead
        // TAIL is index 4
        if (servo_pins_[TAIL] != -1) {
            servo_[TAIL].SetPosition(pos);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    // 6. Clean Finish
    // Move slightly to 0 to ensure dead-center stop
    if (servo_pins_[TAIL] != -1) {
        servo_[TAIL].SetPosition(0);
    }
}

void Puppy::Jump(float steps, int period)
{
    // Placeholder for Jump
    // Quadruped jump is complex, maybe just sit and stand quickly?
    int up[SERVO_COUNT] = {0, 0, 0, 0, 0};
    int down[SERVO_COUNT] = {0, 0, 90, 90, 0};

    for (int i = 0; i < steps; i++)
    {
        MoveServos(period / 2, down);
        MoveServos(period / 2, up);
    }
}

void Puppy::Happy()
{
    // -- ENHANCED HAPPY --
    // "Happy Dance" - Quick side-to-side steps + Fast Wag
    
    if (GetRestState() == true) {
        SetRestState(false);
    }
    ESP_LOGI(TAG, "Happy() - Performing Happy Dance!");

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Relative movement for "Tippy Taps"
    // Rapidly shift weight left and right
    for (int i = 0; i < 4; i++)
    {
        MoveRelative(20, 150);  // Lean Right
        vTaskDelay(pdMS_TO_TICKS(50));
        MoveRelative(-20, 150); // Lean Left
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    // Finish with a big wag
    WagTail(150, 45); // Fast, wide wag
    Stand(); 
#else
    // 180: Positional "Tippy Taps"
    // Lean Left / Right quickly
    int lean_left[SERVO_COUNT] = {-15, 15, -15, 15, 30}; // Tail right
    int lean_right[SERVO_COUNT] = {15, -15, 15, -15, -30}; // Tail left
    int center[SERVO_COUNT] = {0, 0, 0, 0, 0};

    // 1. Tippy Taps (Excited steps)
    for (int i = 0; i < 4; i++)
    {
        MoveServos(150, lean_left);
        MoveServos(150, lean_right);
    }
    
    // 2. Big Wag while standing
    MoveServos(200, center);
    WagTail(150, 45); // Fast, wide wag
#endif
}

void Puppy::Shake()
{
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Use velocity-based small movements
    MoveRelative(20, 100);
    MoveRelative(-20, 100);
    MoveRelative(20, 100);
    MoveRelative(-20, 100);
    Stand();  // Return to stand after behavior
#else
    // 180: Direct positional control
    int left[SERVO_COUNT] = {-20, -20, -20, -20, 0};
    int right[SERVO_COUNT] = {20, 20, 20, 20, 0};
    int stand[SERVO_COUNT] = {0, 0, 0, 0, 0};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(100, left);
        MoveServos(100, right);
    }
    MoveServos(200, stand);
#endif
}

void Puppy::ShakeHands()
{
    // -- SHAKE HANDS --
    // First stand upright, then raise front right leg for handshake
    
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
    ESP_LOGI(TAG, "ShakeHands: Standing first, then raise FR leg");

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Start from Stand (0 degrees)
    Stand();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Crouch slightly on rear legs (-30) while raising front right (+60)
    // Use MoveToAngle for uniform rear legs, then individual control for FR
    MoveToAngle(-30, 60);  // All legs go back
    
    // Move FR forward (relative to current position)
    // FR needs to go from -30 to +60 = +90 delta
    MoveRelative(90, 60);  // This moves all legs but we want just FR
    
    // Hold for 20 seconds
    ESP_LOGI(TAG, "ShakeHands: Holding for 20 seconds...");
    vTaskDelay(pdMS_TO_TICKS(20000));
    
    // Return to stand
    Stand();
#else
    // 180: Direct positional control
    int shake_pose[SERVO_COUNT] = {0, 60, -30, -30, 0};
    MoveServos(1000, shake_pose);
    vTaskDelay(pdMS_TO_TICKS(20000));
    int stand[SERVO_COUNT] = {0, 0, 0, 0, 0};
    MoveServos(1000, stand);
#endif
    
    ESP_LOGI(TAG, "ShakeHands: Finished.");
}

void Puppy::Comfort()
{
    // Sit and lean forward gently
    int sit_lean[SERVO_COUNT] = {30, 30, 80, 80, -10};
    MoveServos(2000, sit_lean);

    // Nuzzle / Sway slowly
    int sway1[SERVO_COUNT] = {40, 20, 80, 80, -15};
    int sway2[SERVO_COUNT] = {20, 40, 80, 80, -5};

    for (int i = 0; i < 3; i++)
    {
        MoveServos(1500, sway1);
        MoveServos(1500, sway2);
    }

    Stand(); // Enforce Stand
}

void Puppy::Excited()
{
    // Fast jumps / tippy taps
    int tap_left[SERVO_COUNT] = {-20, 0, 0, 0, 40};
    int tap_right[SERVO_COUNT] = {0, -20, 0, 0, -40};

    for (int i = 0; i < 6; i++)
    {
        MoveServos(100, tap_left);
        MoveServos(100, tap_right);
    }

    // Big jump
    Jump(1, 500);
    WagTail(100, 40);
    Stand();
}

void Puppy::Cry()
{
    // Sad pose
    int sad_pos[SERVO_COUNT] = {40, 40, 10, 10, -30};
    MoveServos(1500, sad_pos);

    // Sobbing (small rapid movements)
    int sob_up[SERVO_COUNT] = {45, 45, 10, 10, -35};
    int sob_down[SERVO_COUNT] = {35, 35, 10, 10, -25};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(100, sob_up);
        MoveServos(100, sob_down);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    Stand();
}

void Puppy::Sad()
{
    // -- ENHANCED SAD --
    // Crouch low, Head down, Tail Stops/Droops
    
    if (GetRestState() == true) {
        SetRestState(false);
    }
    ESP_LOGI(TAG, "Sad() - Crouching low, tail stop.");

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Slow crouch
    MoveRelative(-40, 30); // Very Slow down
    
    // Stop Tail (or move entirely to one side?)
    // For 360, "0" velocity is stop.
    if (servo_pins_[TAIL] != -1) {
        servo_[TAIL].SetPosition(0); 
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000)); // Stay sad for 3s
    MoveRelative(40, 30); // Slow up
    Stand();
#else
    // 180: Positional
    // Crouch low + Tail Tucked (-45)
    // Head down -> Front legs bent more than rear?
    // Let's try uniform low crouch first.
    int sad_pos[SERVO_COUNT] = {-45, -45, -45, -45, -45}; // Tail tucked down/left
    
    MoveServos(4000, sad_pos); // Very slow transition (4s)
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    Stand();
#endif
}

void Puppy::Angry()
{
    // Aggressive stance, stomp
    int lean_fwd[SERVO_COUNT] = {40, 40, -20, -20, 40}; // Lean forward, tail up
    MoveServos(200, lean_fwd);

    // Stomp front legs
    int stomp_left[SERVO_COUNT] = {0, 40, -20, -20, 45};
    int stomp_right[SERVO_COUNT] = {40, 0, -20, -20, 35};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(100, stomp_left);
        MoveServos(100, lean_fwd);
        MoveServos(100, stomp_right);
        MoveServos(100, lean_fwd);
    }
    Stand();
}

void Puppy::Annoyed()
{
    // Turn away and hold
    int turn_away[SERVO_COUNT] = {20, -20, 20, -20, 10}; // Turn body
    MoveServos(500, turn_away);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Shake head/body briefly
    int shake1[SERVO_COUNT] = {30, -10, 30, -10, 20};
    int shake2[SERVO_COUNT] = {10, -30, 10, -30, 0};

    for (int i = 0; i < 3; i++)
    {
        MoveServos(100, shake1);
        MoveServos(100, shake2);
    }
    MoveServos(500, turn_away);
    vTaskDelay(pdMS_TO_TICKS(500));
    Stand();
}

void Puppy::Shy()
{
    // -- ENHANCED SHY --
    // Crouch low, Turn head away/Cover face with one leg
    
    if (GetRestState() == true) {
        SetRestState(false);
    }
    ESP_LOGI(TAG, "Shy() - Hiding face...");

    int crouch[SERVO_COUNT] = {60, 60, 60, 60, -45}; // Low crouch, tail tucked
    MoveServos(2000, crouch);

#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360: Hard to hold leg up without feedback.
    // Just crouch and wiggle slightly
    for (int i = 0; i < 3; i++) {
        MoveRelative(10, 50);
        vTaskDelay(pdMS_TO_TICKS(200));
        MoveRelative(-10, 50);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
#else
    // 180: Positional
    // Lift Front Right Leg high to "cover face" (approx 90 deg relative to body)
    // Body is crouched at 60.
    // To lift leg, we decrease angle (0 is vertical, -45 is forward/up?)
    // Actually, on most quadrupeds: 0=Vertical, 90=Back, -90=Forward/Up
    
    int hide_face[SERVO_COUNT] = {60, -20, 60, 60, -45}; // FR leg moves to -20 (Forward/Up)
    // Others stay at 60 (Crouched)
    
    MoveServos(1000, hide_face);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Hold pose
    
    // Peek out?
    MoveServos(500, crouch); // Lower leg
    vTaskDelay(pdMS_TO_TICKS(500));
    MoveServos(500, hide_face); // Hide again
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    Stand();
}

void Puppy::Sleepy()
{
    // Lie down completely
    int lie_down[SERVO_COUNT] = {80, 80, 80, 80, -10};
    MoveServos(3000, lie_down);

    // Breathing motion
    int breathe_in[SERVO_COUNT] = {75, 75, 75, 75, -10};
    int breathe_out[SERVO_COUNT] = {80, 80, 80, 80, -10};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(2000, breathe_in);
        vTaskDelay(pdMS_TO_TICKS(500));
        MoveServos(2000, breathe_out);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Stand(); // Return to vertical standing after behavior
}

void Puppy::Calibrate()
{
    // ============================================
    // CALIBRATION FOR MG90S 360° CONTINUOUS SERVOS
    // ============================================
    // Since 360° servos have NO position feedback,
    // calibration works by:
    // 1. STOPPING all motors
    // 2. RESETTING angle tracking to 0 (vertical)
    // 3. User MANUALLY adjusts legs if needed
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "     SERVO CALIBRATION STARTING        ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // 1. Wake up servos if resting
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
    // 2. STOP all servos immediately
    ESP_LOGI(TAG, "Stopping all servo motors...");
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Neutral();  // 1500us = STOP
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 3. Reset ALL position tracking to 0 (vertical)
    ESP_LOGI(TAG, "Resetting angle tracking to 0 degrees...");
    for (int i = 0; i < SERVO_COUNT; i++) {
        estimated_angle_[i] = 0.0f;
        servo_[i].SyncPosition(0.0f);  // Sync oscillator state to 0
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "     CALIBRATION COMPLETE!             ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "All 4 legs set to VERTICAL (0 deg), Tail UPRIGHT (0 deg).");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> IF ANY LEG IS NOT VERTICAL <<<");
    ESP_LOGI(TAG, "1. Use a small screwdriver to loosen the servo horn screw");
    ESP_LOGI(TAG, "2. Rotate the leg to point STRAIGHT DOWN (perpendicular to ground)");
    ESP_LOGI(TAG, "3. Tighten the screw firmly");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> IF TAIL IS NOT UPRIGHT <<<");
    ESP_LOGI(TAG, "Adjust tail horn so it points STRAIGHT UP");
    ESP_LOGI(TAG, "");
}


// Stand: Move ALL servos to upright position (0,0,0,0,0)
// - 4 legs: perpendicular to ground (vertical)
// - Tail: pointing upright
void Puppy::Stand()
{
    // Ensure attached
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
    ESP_LOGI(TAG, "Stand() - Moving ALL servos to VERTICAL (0 deg)");
    
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Use velocity-based movement
    // Move ALL servos (including TAIL) to 0 degrees
    float velocities[SERVO_COUNT] = {0};
    float max_duration = 0;
    int speed_deg_per_sec = 45; // Faster for responsive standing
    
    for(int i = 0; i < SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1) continue;
        
        float current = estimated_angle_[i];
        float delta = 0.0f - current;  // Target is always 0
        
        // Skip if already very close to target
        if(std::abs(delta) < 0.5f) continue;
        
        float duration_ms = (std::abs(delta) / (float)speed_deg_per_sec) * 1000.0f;
        if(duration_ms > max_duration) max_duration = duration_ms;
        
        float direction = (delta > 0) ? 1.0f : -1.0f;
        float pwm = ((float)speed_deg_per_sec / 360.0f) * direction;
        pwm *= servo_speed_scale_[i];
        
        velocities[i] = pwm;
    }
    
    if (max_duration > 0) {
        ESP_LOGI(TAG, "Stand() [360] Duration=%.0fms", max_duration);
        MoveServosVelocity((int)max_duration, velocities);
    } else {
        ESP_LOGI(TAG, "Stand() [360] Already at 0 - No movement needed");
    }
    
    // ALWAYS reset estimated angles to 0 (including TAIL) after stand
    for(int i = 0; i < SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1) {
            estimated_angle_[i] = 0.0f;
            servo_[i].SyncPosition(0.0f);  // Sync oscillator state to 0
        }
    }
    
    ESP_LOGI(TAG, "Stand() Complete - All servos at 0 degrees (vertical)");
    
#else
    // 180: Direct positional control
    ESP_LOGI(TAG, "Stand() [180] All servos to 0.");
    int stand_pos[SERVO_COUNT] = {0, 0, 0, 0, 0}; 
    MoveServos(1000, stand_pos);
#endif
    // DO NOT Detach/Home. Stay active to hold weight.
}

void Puppy::GentleStand(bool force)
{
    // Ensure attached
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
    ESP_LOGI(TAG, "GentleStand(force=%d) - Checking current pose...", force);

    if (!force) {
        // Check if already standing (all legs close to 0)
        bool already_standing = true;
        for(int i = 0; i < SERVO_COUNT; i++) {
            if(servo_pins_[i] != -1 && i != TAIL) { // Ignore tail for standing check
                if (std::abs(estimated_angle_[i]) > 5.0f) {
                    already_standing = false;
                    break;
                }
            }
        }

        if (already_standing) {
            ESP_LOGI(TAG, "GentleStand() - Already standing. Skipping.");
            return;
        }
    }

    ESP_LOGI(TAG, "GentleStand() - Moving to VERTICAL (0 deg)...");
    
#if defined(CONFIG_PUPPY_SERVO_TYPE_360_CONT) || defined(CONFIG_PUPPY_SERVO_TYPE_360_POS)
    // 360 servos: Use velocity-based movement with LOWER speed
    float velocities[SERVO_COUNT] = {0};
    float max_duration = 0;
    int speed_deg_per_sec = 20; // Slower speed for gentle startup
    
    for(int i = 0; i < SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1) continue;
        
        float current = estimated_angle_[i];
        float delta = 0.0f - current;  // Target is always 0
        
        // Skip if already very close to target
        if(std::abs(delta) < 0.5f) continue;
        
        float duration_ms = (std::abs(delta) / (float)speed_deg_per_sec) * 1000.0f;
        if(duration_ms > max_duration) max_duration = duration_ms;
        
        float direction = (delta > 0) ? 1.0f : -1.0f;
        float pwm = ((float)speed_deg_per_sec / 360.0f) * direction;
        pwm *= servo_speed_scale_[i];
        
        velocities[i] = pwm;
    }
    
    if (max_duration > 0) {
        ESP_LOGI(TAG, "GentleStand() [360] Duration=%.0fms", max_duration);
        MoveServosVelocity((int)max_duration, velocities);
    } else {
        ESP_LOGI(TAG, "GentleStand() [360] Already at 0");
    }
    
    // ALWAYS reset estimated angles to 0 (including TAIL) after stand
    for(int i = 0; i < SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1) {
            estimated_angle_[i] = 0.0f;
            servo_[i].SyncPosition(0.0f);  // Sync oscillator state to 0
        }
    }
    
#else
    // 180: Direct positional control with LONGER duration
    ESP_LOGI(TAG, "GentleStand() [180] All servos to 0 over 3 seconds.");
    int stand_pos[SERVO_COUNT] = {0, 0, 0, 0, 0}; 
    MoveServos(3000, stand_pos); // 3000ms = 3 seconds
#endif
}



void Puppy::WelcomeWag()
{
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
    // Safety check: Is tail connected?
    if (servo_pins_[TAIL] == -1) return;

    ESP_LOGI(TAG, "WelcomeWag: Starting Realistic Greeting Sequence...");

    // Keyframes for "Nung Niu" (Affectionate) Wag
    // 1. Gentle Hello (Slow, Small)
    // 2. Excited "It's You!" (Fast, Wide)
    // 3. Settling Down (Medium, Small)
    
    struct Keyframe {
        int target_angle;
        int duration_ms;
    };
    
    Keyframe sequence[] = {
        // --- Swing 1: Gentle greeting ---
        {20, 300},   // Tilt right slowly
        {0,  300},   // Return slowly
        
        // --- Swing 2: EXCITEMENT! ---
        {-35, 150},  // Fast Left
        {35,  150},  // Fast Right
        {-35, 150},  // Fast Left Again
        {0,   200},  // Return to Center
        
        // --- Swing 3: Happy wiggle ---
        {15, 250},   // Slight right
        {0,  350}    // Settle back to 0
    };
    
    int steps = sizeof(sequence) / sizeof(sequence[0]);
    int start_angle = 0; // Assuming started at 0 (Stand)

    for(int k=0; k<steps; k++) {
        int target = sequence[k].target_angle;
        int total_time = sequence[k].duration_ms;
        
        // Simple interpolation loop
        int update_interval = 20; // 50Hz
        int frames = total_time / update_interval;
        if (frames < 1) frames = 1;
        
        for(int f=1; f<=frames; f++) {
            float progress = (float)f / (float)frames;
            
            // Linear is fine for short bursts, but let's smooth it slightly
            // float smooth_p = progress * progress * (3 - 2 * progress); // SmoothStep if desired
            
            int current_pos = start_angle + (int)((target - start_angle) * progress);
            
            servo_[TAIL].SetPosition(current_pos);
            vTaskDelay(pdMS_TO_TICKS(update_interval));
        }
        start_angle = target;
    }
    
    // Hard lock to 0 at the end to be sure
    servo_[TAIL].SetPosition(0);
    ESP_LOGI(TAG, "WelcomeWag: Finished.");
}
