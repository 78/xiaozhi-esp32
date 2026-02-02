#include "genu_movements.h"
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "GenuMovements";

// Helper for millis
unsigned long IRAM_ATTR millis()
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

// Convert degrees to radians
#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

GenuRobot::GenuRobot()
{
    is_resting_ = false;
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
    }
}

GenuRobot::~GenuRobot()
{
    DetachServos();
}

void GenuRobot::Init(int head_pin, int left_arm_pin, int right_arm_pin)
{
    servo_pins_[HEAD] = head_pin;
    servo_pins_[LEFT_ARM] = left_arm_pin;
    servo_pins_[RIGHT_ARM] = right_arm_pin;

    AttachServos();
    Home();
}

void GenuRobot::AttachServos()
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            // Start channels from 2 to avoid conflicts (0/1 often used for backlight/buzzers if applicable)
            // But we can just verify collision. Oscillator class handles attach.
            servo_[i].Attach(servo_pins_[i], i + 2);
        }
    }
    is_resting_ = false;
}

void GenuRobot::DetachServos()
{
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].Detach();
        }
    }
    is_resting_ = true;
}

void GenuRobot::MoveServos(int time, int servo_target[])
{
    if (is_resting_) AttachServos();

    // Limit execution time to avoid divide by zero
    if (time < 10) time = 10;
    
    // Linear interpolation
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            increment_[i] = ((double)(servo_target[i] - servo_[i].GetPosition())) / (time / 10.0);
        }
    }

    unsigned long start_time = millis();
    unsigned long end_time = start_time + time;

    while (millis() < end_time)
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
    
    // Ensure final position
    for (int i = 0; i < SERVO_COUNT; i++)
    {
        if (servo_pins_[i] != -1)
        {
            servo_[i].SetPosition(servo_target[i]);
        }
    }
}

void GenuRobot::MoveSingle(int position, int servo_number)
{
    if (is_resting_) AttachServos();
    
    if (servo_number >= 0 && servo_number < SERVO_COUNT && servo_pins_[servo_number] != -1)
    {
        servo_[servo_number].SetPosition(position);
    }
}

void GenuRobot::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period, double phase_diff[SERVO_COUNT], float cycle)
{
    if (is_resting_) AttachServos();

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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void GenuRobot::Home()
{
    int homes[SERVO_COUNT] = {0, 0, 0}; // Head=0 (Center), Arms=0 (Down)
    MoveServos(1000, homes);
    DetachServos(); // Save power
}

void GenuRobot::Happy()
{
    // Happy dance: Arms up/down, Head nodding
    // Head: Small nod, Arms: Big wave
    int amp[SERVO_COUNT] = {10, 40, 40};
    int off[SERVO_COUNT] = {0, 30, 30}; // Arms slightly up
    double phase[SERVO_COUNT] = {0, 0, DEG2RAD(180)}; // Arms opposite
    
    OscillateServos(amp, off, 500, phase, 4); // 4 cycles, fast
    Home();
}

void GenuRobot::Sad()
{
    // Head down, Arms limp
    int sad_pos[SERVO_COUNT] = {20, 10, 10}; // Head down (positive?), Arms slightly in
    MoveServos(2000, sad_pos);
    vTaskDelay(pdMS_TO_TICKS(2000));
    Home();
}

void GenuRobot::Angry()
{
    // Shaking head "No", Arms stiff
    int amp[SERVO_COUNT] = {30, 0, 0};
    int off[SERVO_COUNT] = {0, 10, 10}; // Arms stiff
    double phase[SERVO_COUNT] = {0, 0, 0};
    
    OscillateServos(amp, off, 200, phase, 6); // Fast shake
    Home();
}

void GenuRobot::Wave()
{
    // One arm wave (Right Arm)
    int amp[SERVO_COUNT] = {0, 0, 40};
    int off[SERVO_COUNT] = {0, 0, 40}; // Right arm up
    double phase[SERVO_COUNT] = {0, 0, 0};
    
    // Lift arm first
    int lift[SERVO_COUNT] = {0, 0, 80};
    MoveServos(500, lift);
    
    OscillateServos(amp, off, 300, phase, 4);
    Home();
}

void GenuRobot::Dance()
{
    // Disco: Arms up/down alternating, Head swaying
    int amp[SERVO_COUNT] = {20, 60, 60};
    int off[SERVO_COUNT] = {0, 20, 20};
    double phase[SERVO_COUNT] = {0, 0, DEG2RAD(180)};
    
    OscillateServos(amp, off, 800, phase, 6);
    Home();
}

void GenuRobot::Comfort()
{
    // Hug: Arms open wide then close
    int open[SERVO_COUNT] = {-10, 80, 80}; // Head up, arms out
    MoveServos(1000, open);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    int hug[SERVO_COUNT] = {10, 20, 20}; // Head down, arms in
    MoveServos(1000, hug);
    vTaskDelay(pdMS_TO_TICKS(2000));
    Home();
}

void GenuRobot::Excited()
{
    // Fast tapping/waving
    int amp[SERVO_COUNT] = {10, 30, 30};
    int off[SERVO_COUNT] = {0, 40, 40};
    double phase[SERVO_COUNT] = {0, 0, 0}; // Both arms same
    
    OscillateServos(amp, off, 200, phase, 10);
    Home();
}

void GenuRobot::Shy()
{
    // Hide face: Arms up to face, Head down
    int hide[SERVO_COUNT] = {30, 70, 70}; // Head down, Arms up covering
    MoveServos(2000, hide);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Peek
    int peek[SERVO_COUNT] = {0, 50, 50};
    MoveServos(1000, peek);
    vTaskDelay(pdMS_TO_TICKS(500));
    MoveServos(1000, hide);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    Home();
}

void GenuRobot::Sleepy()
{
    // Slow nod
    int amp[SERVO_COUNT] = {20, 0, 0};
    int off[SERVO_COUNT] = {10, 0, 0};
    double phase[SERVO_COUNT] = {0, 0, 0};
    
    OscillateServos(amp, off, 3000, phase, 3);
    Home();
}
