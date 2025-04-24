#ifndef __OTTO_MOVEMENTS_H__
#define __OTTO_MOVEMENTS_H__

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

class Otto {
public:
    Otto();
    ~Otto();

    //-- Otto initialization
    void Init(int left_leg, int right_leg, int left_foot, int right_foot);
    //-- Attach & detach functions
    void AttachServos();
    void DetachServos();

    //-- Oscillator Trims
    void SetTrims(int left_leg, int right_leg, int left_foot, int right_foot);

    //-- Predetermined Motion Functions
    void MoveServos(int time, int servo_target[]);
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[4], int offset[4], int period, double phase_diff[4],
                         float cycle);

    //-- HOME = Otto at rest position
    void Home();
    bool GetRestState();
    void SetRestState(bool state);

    //-- Predetermined Motion Functions
    void Jump(float steps = 1, int period = 2000);

    void Walk(float steps = 4, int period = 1000, int dir = FORWARD);
    void Turn(float steps = 4, int period = 2000, int dir = LEFT);
    void Bend(int steps = 1, int period = 1400, int dir = LEFT);
    void ShakeLeg(int steps = 1, int period = 2000, int dir = RIGHT);

    void UpDown(float steps = 1, int period = 1000, int height = 20);
    void Swing(float steps = 1, int period = 1000, int height = 20);
    void TiptoeSwing(float steps = 1, int period = 900, int height = 20);
    void Jitter(float steps = 1, int period = 500, int height = 20);
    void AscendingTurn(float steps = 1, int period = 900, int height = 20);

    void Moonwalker(float steps = 1, int period = 900, int height = 20, int dir = LEFT);
    void Crusaito(float steps = 1, int period = 900, int height = 20, int dir = FORWARD);
    void Flapping(float steps = 1, int period = 1000, int height = 20, int dir = FORWARD);

    // -- Servo limiter
    void EnableServoLimit(int speed_limit_degree_per_sec = SERVO_LIMIT_DEFAULT);
    void DisableServoLimit();

private:
    Oscillator servo_[4];

    int servo_pins_[4];
    int servo_trim_[4];

    unsigned long final_time_;
    unsigned long partial_time_;
    float increment_[4];

    bool is_otto_resting_;

    void Execute(int amplitude[4], int offset[4], int period, double phase_diff[4], float steps);
};

#endif  // __OTTO_MOVEMENTS_H__