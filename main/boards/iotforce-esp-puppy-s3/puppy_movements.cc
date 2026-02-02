#include "puppy_movements.h"
#include <algorithm>
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


    // For Continuous servos, MoveServos (Positional) logic is adapted via Oscillator::Write
    // which converts position delta to velocity if configured.
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

// 360 Servo: Move Relative Time-Based
void Puppy::MoveRelative(int relative_angle, int speed_deg_per_sec)
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    if (speed_deg_per_sec <= 0) speed_deg_per_sec = 60;

    int valid_servos = 0;
    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] != -1 && i != TAIL) valid_servos++;
    }
    if(valid_servos == 0) return;

    // Calculate Duration
    float duration_ms = (abs(relative_angle) / (float)speed_deg_per_sec) * 1000.0f;
    
    // Calculate Velocity Direction
    // Left Side: +Delta = Forward (CCW?) -> Positive PWM
    // Right Side: +Delta = Forward (CW?) -> Negative PWM (Mirrored)
    
    float velocities[SERVO_COUNT] = {0};
    
    float direction = (relative_angle > 0) ? 1.0f : -1.0f;
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
        
        // Optimistic Update
        estimated_angle_[i] += relative_angle;
    }
    
    ESP_LOGI(TAG, "MoveRelative: Angle=%d Duration=%.0f", relative_angle, duration_ms);
    // Execute
    MoveServosVelocity((int)duration_ms, velocities);
#endif
}

void Puppy::MoveToAngle(int target_angle, int speed_deg_per_sec)
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // For 360 servos, MoveToAngle relies on "estimated_angle_" which drifts.
    // However, it's useful if we assume we just completed a move.
    
    // Calculate max delta needed
    float max_delta = 0;
    
    // We can't easily move all legs to different angles with MoveRelative 
    // unless we implement per-servo relative logic. 
    // For simplicity, let's treat MoveToAngle as "Move All Legs to this Angle assuming they are roughly synced"
    // OR, better yet, calculate per-leg delta.
    
    float velocities[SERVO_COUNT] = {0};
    float max_duration = 0;
    
    if (speed_deg_per_sec <= 0) speed_deg_per_sec = 60;

    for(int i=0; i<SERVO_COUNT; i++) {
        if(servo_pins_[i] == -1 || i == TAIL) continue;

        float current = estimated_angle_[i];
        float delta = target_angle - current;
        
        float duration_ms = (abs(delta) / (float)speed_deg_per_sec) * 1000.0f;
        if(duration_ms > max_duration) max_duration = duration_ms;
        
        float direction = (delta > 0) ? 1.0f : -1.0f;
        float pwm = ((float)speed_deg_per_sec / 360.0f) * direction;
        
        // Apply Speed Scale
        pwm *= servo_speed_scale_[i];
        
        if (i == FR_LEG || i == BR_LEG) pwm = -pwm;
        
        velocities[i] = pwm;
        estimated_angle_[i] = target_angle;
    }
    
    if (max_duration > 0) {
        ESP_LOGI(TAG, "MoveToAngle: Target=%d Duration=%.0f", target_angle, max_duration);
        MoveServosVelocity((int)max_duration, velocities);
    }
#else
    // Fallback for Positional Servos
    int targets[SERVO_COUNT];
    for(int i=0; i<SERVO_COUNT; i++) targets[i] = target_angle; // This is crude, positional mode usually passes array
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

#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // -- 360 Mode Home --
    // Move to Stand (0 degrees) to reset any accumulated drift from special actions.
    ESP_LOGI(TAG, "Home() Triggered. Resetting to Stand (0).");
    MoveToAngle(0, 60);
    // float velocity[SERVO_COUNT] = {0, 0, 0, 0, 0};
    // MoveServosVelocity(100, velocity);
#else


    int homes[SERVO_COUNT] = {0, 0, 0, 0, 0}; // All servos to 0 degrees (center)
    MoveServos(1000, homes); // Slow home move (1s)
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

#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // Action: SIT DOWN
    // User requested "Forward 90 to Stand". So "Backward 90 to Sit".
    // We used 60 before. Let's use -60 (Backward).
    ESP_LOGI(TAG, "Sit() Triggered. Target -60.");
    MoveToAngle(-60, 45); 

#else
    int sit_pos[SERVO_COUNT] = {0, 0, 90, 90, 0}; 
    MoveServos(1000, sit_pos);
#endif
}



void Puppy::WagTail(int period, int amplitude)
{
    int amp[SERVO_COUNT] = {0, 0, 0, 0, amplitude};
    int off[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase[SERVO_COUNT] = {0, 0, 0, 0, 0};

    OscillateServos(amp, off, period, phase, 5); // Wag 5 times
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
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // 360 Happy: Crouch and Jump
    // Crouch: Move Legs Backward (like Sit) -> -30
    // Jump: Move Legs Forward (Stand) -> +30
    
    for (int i = 0; i < 3; i++)
    {
        MoveRelative(-30, 200);  // Crouch (Back)
        vTaskDelay(pdMS_TO_TICKS(100));
        MoveRelative(30, 200);   // Stand (Fwd)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    WagTail(200, 40);
#else
    // Crouch and Jump 3 times
    int crouch[SERVO_COUNT] = {45, 45, 45, 45, 0}; // All legs bent
    int stand[SERVO_COUNT] = {0, 0, 0, 0, 0};      // All legs straight

    for (int i = 0; i < 3; i++)
    {
        MoveServos(300, crouch); // Crouch down
        MoveServos(200, stand);  // Jump up
    }

    // Wag Tail
    WagTail(200, 40);
#endif
}

void Puppy::Shake()
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // 360 Shake: Just wiggle little bit?
    // It's hard to shake body laterally without correct kinematics.
    // We'll mimic by small fwd/back offsets using Walk gait logic but inplace?
    // Or just skip for now, maybe small hops?
    // Let's do small "Bow"
    MoveRelative(20, 100);
    MoveRelative(-20, 100);
    MoveRelative(20, 100);
    MoveRelative(-20, 100);
#else
    // Shake body left/right
    int left[SERVO_COUNT] = {-20, -20, -20, -20, 0};
    int right[SERVO_COUNT] = {20, 20, 20, 20, 0};
    int stand[SERVO_COUNT] = {0, 0, 0, 0, 0};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(100, left);
        MoveServos(100, right);
    }
    MoveServos(200, stand); // Back to home
#endif
}

void Puppy::ShakeHands()
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // Sit first
    Sit();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Raise FR_LEG (Index 1)
    
    // Raise Leg: Move Negative (Back/Up?) or Positive (Fwd/Down?)
    // If Sit is -60. Stand is 0.
    // Raising a leg usually means rotating it BACK/UP.
    // So more Negative? or Positive?
    // If Stand(0) -> Sit(-60).
    // Raising Hand -> Maybe -90?
    
    float velocity[SERVO_COUNT] = {0};
    velocity[FR_LEG] = -0.5f; // "Back/Up" (Assuming Negative is Up/Back)
    MoveServosVelocity(300, velocity); 
    
    // Shake
    for(int i=0; i<3; i++) {
        velocity[FR_LEG] = 0.3f; // Fwd/Down
        MoveServosVelocity(150, velocity);
        velocity[FR_LEG] = -0.3f; // Back/Up
        MoveServosVelocity(150, velocity);
    }
    
    // Put Down (Return to Sit = -60)
    // Explicitly call Sit() to realign all legs to the standard Sitting pose.
    Sit();
    
    // velocity[FR_LEG] = 0.3f; 
    // MoveServosVelocity(300, velocity); 
    
    // // Reset estimated?
    // estimated_angle_[FR_LEG] = -60; 
#else
    // Sit first
    int sit_pos[SERVO_COUNT] = {0, 0, 90, 90, 0};
    MoveServos(1000, sit_pos);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Raise Right Front Leg (FR_LEG is index 1)
    // And wag tail
    int raise_hand[SERVO_COUNT] = {0, 60, 90, 90, 20}; // FR_LEG up
    MoveServos(500, raise_hand);

    // Shake hand and wag tail
    int shake_in[SERVO_COUNT] = {0, 70, 90, 90, -20};
    int shake_out[SERVO_COUNT] = {0, 50, 90, 90, 20};

    for (int i = 0; i < 5; i++)
    {
        MoveServos(150, shake_in);
        MoveServos(150, shake_out);
    }

    // Put down
    MoveServos(500, sit_pos);

    // Wait 10 seconds before returning to normal state
    vTaskDelay(pdMS_TO_TICKS(10000));

    Home();
#endif
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
    Home();
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
    Home();
}

void Puppy::Sad()
{
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // Sad: Head down / Slow Bow
    // Bow = Front Legs Out (Forward) or Back (Crouch)?
    // Usually Sad = Low. Crouch (-30).
    MoveRelative(-30, 20); // Slow bend down
    vTaskDelay(pdMS_TO_TICKS(2000));
    MoveRelative(30, 20); // Slow up
    Home();
#else
    // Head down, slow movement
    int sad_pos[SERVO_COUNT] = {30, 30, 0, 0, -20}; // Front legs bent, tail down
    MoveServos(2000, sad_pos);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Slight sway
    int sad_sway1[SERVO_COUNT] = {35, 25, 0, 0, -25};
    int sad_sway2[SERVO_COUNT] = {25, 35, 0, 0, -15};

    for (int i = 0; i < 3; i++)
    {
        MoveServos(1000, sad_sway1);
        MoveServos(1000, sad_sway2);
    }

    Home();
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
    Home();
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
    Home();
}

void Puppy::Shy()
{
    // Crouch low and slow
    int crouch[SERVO_COUNT] = {60, 60, 60, 60, -40}; // Low crouch, tail tucked
    MoveServos(2000, crouch);

    // Hide face? (Move front legs in?)
    int hide[SERVO_COUNT] = {60, 60, 60, 60, -45}; // Just slight movement

    for (int i = 0; i < 3; i++)
    {
        MoveServos(500, hide);
        MoveServos(500, crouch);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    Home();
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
    Home();
}

void Puppy::Calibrate()
{
    // Cân chỉnh động cơ về góc 0 (Stand/Sit).
    // User must manually place robot in STAND position (Vertical legs).
    
    ESP_LOGI(TAG, "Calibration: Relaxing Limiters. Please manually set legs to STAND (Vertical/0 deg) within 3 seconds.");
    
    // 1. Relax: Detach servos so they can be moved freely
    DetachServos();
    
    // 2. Wait for user adjustment (3 seconds)
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 3. Reset internal angles to 0 (Stand)
    for(int i=0; i<SERVO_COUNT; i++) {
        estimated_angle_[i] = 0.0f;
        // Reset low-level oscillator state to prevent jerk/spin
        servo_[i].ResetPosition();
    }
    
    // 4. Re-attach and Hold
    AttachServos();
    
    ESP_LOGI(TAG, "Calibration: Locked at 0 (Stand).");
}

// Stand: Move to upright position (0,0,0,0,0) and STAY ENGAGED.
// Stand: Move to upright position (0,0,0,0,0) and STAY ENGAGED.
void Puppy::Stand()
{
    // Ensure attached
    if (GetRestState() == true) {
        SetRestState(false);
    }
    
#ifdef CONFIG_PUPPY_SERVO_TYPE_360_CONT
    // -- 360 Mode Stand --
    // Target 0 degrees (Upright/Vertical).
    // IMPORTANT: "Wake Up" calls this. User said it walks endlessly.
    // This is because we used MoveToAngle, which sets a velocity based on estimated delta.
    
    // If we are "waking up", we assume we are coming from SIT (60 deg) or SLEEP (80 deg).
    // So target is 0.
    // If we are "waking up", we assume we are coming from SIT (60 deg) or SLEEP (80 deg).
    // So target is 0.
    ESP_LOGI(TAG, "Stand() Triggered. Target 0.");
    MoveToAngle(0, 60); 
    
    // FORCE STOP after move to prevent drift/walking?
    // MoveToAngle already stops (MoveServosVelocity stops after duration).
    
#else
    int stand_pos[SERVO_COUNT] = {0, 0, 0, 0, 0}; 
    MoveServos(1000, stand_pos);
#endif
    // DO NOT Detach/Home. Stay active to hold weight.
}
