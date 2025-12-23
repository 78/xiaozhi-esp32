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
            servo_[i].Attach(servo_pins_[i]);
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
            partial_time_ = millis() + 10;
            for (int i = 0; i < SERVO_COUNT; i++)
            {
                if (servo_pins_[i] != -1)
                {
                    servo_[i].SetPosition(servo_[i].GetPosition() + increment_[i]);
                }
            }
            while (millis() < partial_time_)
                ; // Pause
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
    if (servo_pins_[servo_number] != -1)
    {
        servo_[servo_number].SetPosition(position);
    }
}

void Puppy::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                            double phase_diff[SERVO_COUNT], float cycle)
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_[i].SetO(offset[i]);
        servo_[i].SetA(amplitude[i]);
        servo_[i].SetT(period);
        servo_[i].SetPh(phase_diff[i]);
    }
    double ref = millis();
    for (double x = ref; x <= ref + period * cycle; x = millis())
    {
        for (int i = 0; i < SERVO_COUNT; i++)
        {
            servo_[i].Refresh();
        }
    }
}

void Puppy::Home()
{
    if (is_puppy_resting_)
        return;

    int homes[SERVO_COUNT] = {0, 0, 0, 0, 0}; // All servos to 0 degrees (center)
    MoveServos(500, homes);
    DetachServos();
    is_puppy_resting_ = true;
}

bool Puppy::GetRestState()
{
    return is_puppy_resting_;
}

void Puppy::SetRestState(bool state)
{
    is_puppy_resting_ = state;
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
    // Basic trot gait
    // FL and BR move together, FR and BL move together
    // Phase diff: FL=0, BR=0, FR=180, BL=180

    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0};
    int offset[SERVO_COUNT] = {0, 0, 0, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0};

    if (dir == FORWARD)
    {
        phase_diff[FL_LEG] = DEG2RAD(0);
        phase_diff[BR_LEG] = DEG2RAD(0);
        phase_diff[FR_LEG] = DEG2RAD(180);
        phase_diff[BL_LEG] = DEG2RAD(180);
    }
    else
    { // BACKWARD
        phase_diff[FL_LEG] = DEG2RAD(180);
        phase_diff[BR_LEG] = DEG2RAD(180);
        phase_diff[FR_LEG] = DEG2RAD(0);
        phase_diff[BL_LEG] = DEG2RAD(0);
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
        offset[FL_LEG] = -20;
        offset[BL_LEG] = -20;
        offset[FR_LEG] = 20;
        offset[BR_LEG] = 20;
    }
    else
    { // RIGHT
        offset[FL_LEG] = 20;
        offset[BL_LEG] = 20;
        offset[FR_LEG] = -20;
        offset[BR_LEG] = -20;
    }

    // Use a walking gait but with offsets to turn
    phase_diff[FL_LEG] = DEG2RAD(0);
    phase_diff[BR_LEG] = DEG2RAD(0);
    phase_diff[FR_LEG] = DEG2RAD(180);
    phase_diff[BL_LEG] = DEG2RAD(180);

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
