#include "puppy_movements.h"
#include <algorithm>
#include "freertos/idf_additions.h"

static const char *TAG = "PuppyMovements";

Puppy::Puppy()
{
    is_puppy_resting_ = false;
    // Initialize all servo pins to -1 (not connected)
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
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

void Puppy::MoveServos(int time, int servo_target[])
{
    if (GetRestState() == true)
    {
        SetRestState(false);
    }

    final_time_ = millis() + time;
    if (time > 10)
    {
        for (int i = 0; i < SERVO_COUNT; i++)
        {
            if (servo_pins_[i] != -1)
            {
                increment_[i] = (servo_target[i] - servo_[i].GetPosition()) / (time / 10.0);
            }
        }
        for (int iteration = 0; iteration < time / 10; iteration++)
        {
            for (int i = 0; i < SERVO_COUNT; i++)
            {
                if (servo_pins_[i] != -1)
                {
                    servo_[i].SetPosition(servo_[i].GetPosition() + increment_[i]);
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
}

void Puppy::MoveSingle(int position, int servo_number)
{
    if (position > 90)
        position = 90;
    if (position < -90)
        position = -90;

    if (GetRestState() == true)
    {
        SetRestState(false);
    }

    if (servo_pins_[servo_number] != -1)
    {
        servo_[servo_number].SetPosition(position);
    }
}

void Puppy::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                            double phase_diff[SERVO_COUNT], float cycle)
{
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
}

void Puppy::Home()
{
    if (is_puppy_resting_)
        return;

    int homes[SERVO_COUNT] = {0, 0, 0, 0, 0}; // All servos to 0 degrees (center)
    MoveServos(500, homes);
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
    // Basic trot gait for 4-servo quadruped
    // FL and BR move together (Diagonal)
    // FR and BL move together (Diagonal)

    // Assuming standard servo orientation (Left: + is Fwd, Right: + is Back)
    // We want FL Fwd (+), BR Fwd (- -> 180)
    // We want FR Back (+ -> 0), BL Back (- -> 180)

    // So phases should be:
    // FL: 0
    // FR: 0
    // BL: 180
    // BR: 180

    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0};
    int offset[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0};

    if (dir == FORWARD)
    {
        phase_diff[FL_LEG] = DEG2RAD(0);
        phase_diff[FR_LEG] = DEG2RAD(0);
        phase_diff[BL_LEG] = DEG2RAD(180);
        phase_diff[BR_LEG] = DEG2RAD(180);
    }
    else
    { // BACKWARD
        phase_diff[FL_LEG] = DEG2RAD(180);
        phase_diff[FR_LEG] = DEG2RAD(180);
        phase_diff[BL_LEG] = DEG2RAD(0);
        phase_diff[BR_LEG] = DEG2RAD(0);
    }

    OscillateServos(amplitude, offset, period, phase_diff, steps);
}

void Puppy::Turn(float steps, int period, int dir)
{
    // Turn by moving legs on one side differently
    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0};
    int offset[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0};

    if (dir == LEFT)
    {
        // Turn Left: Left legs move Back, Right legs move Fwd
        // FL Back (- -> 180)
        // BL Back (- -> 180)
        // FR Fwd (- -> 180)
        // BR Fwd (- -> 180)

        phase_diff[FL_LEG] = DEG2RAD(180);
        phase_diff[FR_LEG] = DEG2RAD(180);
        phase_diff[BL_LEG] = DEG2RAD(180);
        phase_diff[BR_LEG] = DEG2RAD(180);
    }
    else
    { // RIGHT
        // Turn Right: Left legs move Fwd, Right legs move Back
        // FL Fwd (+ -> 0)
        // BL Fwd (+ -> 0)
        // FR Back (+ -> 0)
        // BR Back (+ -> 0)

        phase_diff[FL_LEG] = DEG2RAD(0);
        phase_diff[FR_LEG] = DEG2RAD(0);
        phase_diff[BL_LEG] = DEG2RAD(0);
        phase_diff[BR_LEG] = DEG2RAD(0);
    }

    OscillateServos(amplitude, offset, period, phase_diff, steps);
}

void Puppy::Sit()
{
    int sit_pos[SERVO_COUNT] = {0, 0, 90, 90, 0}; // Back legs bent? Depends on mechanical structure
    // Assuming 90 is bent up/down. Adjust as needed.
    MoveServos(1000, sit_pos);
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
}

void Puppy::Shake()
{
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
}

void Puppy::Sad()
{
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
    // Move each servo one by one to check range/operation
    // Center all first
    int center[SERVO_COUNT] = {0, 0, 0, 0, 0};
    MoveServos(1000, center);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Wiggle each leg
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            MoveSingle(20, i);
            vTaskDelay(pdMS_TO_TICKS(200));
            MoveSingle(-20, i);
            vTaskDelay(pdMS_TO_TICKS(200));
            MoveSingle(0, i);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    Home();
}
