#ifndef __PUPPY_MOVEMENTS_H__
#define __PUPPY_MOVEMENTS_H__

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscillator.h"

//-- Constants
#define FORWARD 1
#define BACKWARD -1
#define LEFT 1
#define RIGHT -1
#define SMALL 5
#define MEDIUM 15
#define BIG 30

// -- Servo delta limit default. degree / sec
#define SERVO_LIMIT_DEFAULT 240

// -- Servo indexes for easy access
#define FL_LEG 0
#define FR_LEG 1
#define BL_LEG 2
#define BR_LEG 3
#define TAIL 4
#define SERVO_COUNT 5

class Puppy
{
public:
    Puppy();
    ~Puppy();

    //-- Puppy initialization
    void Init(int fl_leg, int fr_leg, int bl_leg, int br_leg, int tail = -1);

    //-- Attach & detach functions
    void AttachServos();
    void DetachServos();

    //-- Oscillator Trims
    void SetTrims(int fl_leg, int fr_leg, int bl_leg, int br_leg, int tail = 0);
    void SetSpeedScales(float fl, float fr, float bl, float br, float tail);

    //-- Predetermined Motion Functions
    void MoveServos(int time, int servo_target[]);
    void MoveServosVelocity(int time, float servo_velocity[]); // Velocity -1.0 to 1.0
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                         double phase_diff[SERVO_COUNT], float cycle);

    //-- HOME = Puppy at rest position
    void Home();
    bool GetRestState();
    void SetRestState(bool state);

    //-- Basic Movements
    void Walk(float steps = 4, int period = 1000, int dir = FORWARD);
    void Turn(float steps = 4, int period = 1000, int dir = LEFT);
    void Sit();
    void Stand();
    void WagTail(int period = 500, int amplitude = 30);
    void Jump(float steps = 1, int period = 2000);
    void Happy();
    void Shake();
    void ShakeHands();
    void Comfort();
    void Excited();
    void Cry();
    void Sad();
    void Angry();
    void Annoyed();
    void Shy();
    void Sleepy();
    void Calibrate();
    
    // Virtual Position Tracking for 360 Servos (Now Public for Calibration)
    void MoveToAngle(int target_angle, int speed_deg_per_sec);
    void MoveRelative(int relative_angle, int speed_deg_per_sec);

    // -- Servo limiter
    void EnableServoLimit(int speed_limit_degree_per_sec = SERVO_LIMIT_DEFAULT);
    void DisableServoLimit();

private:
    Oscillator servo_[SERVO_COUNT];
    int servo_pins_[SERVO_COUNT];
    int servo_trim_[SERVO_COUNT];

    bool is_puppy_resting_;
    unsigned long final_time_;
    unsigned long partial_time_;

    // Virtual Position Tracking for 360 Servos
    float estimated_angle_[SERVO_COUNT]; 
    float servo_speed_scale_[SERVO_COUNT];

};

#endif // __PUPPY_MOVEMENTS_H__
