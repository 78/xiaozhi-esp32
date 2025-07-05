#ifndef __MOVEMENTS_H__
#define __MOVEMENTS_H__

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
#define BOTH 0
#define SMALL 5
#define MEDIUM 15
#define BIG 30

// -- Servo delta limit default. degree / sec
#define SERVO_LIMIT_DEFAULT 240

// -- Servo indexes for easy access
#define RIGHT_PITCH 0
#define RIGHT_ROLL 1
#define LEFT_PITCH 2
#define LEFT_ROLL 3
#define BODY 4
#define HEAD 5
#define SERVO_COUNT 6

class Otto {
public:
    Otto();
    ~Otto();

    //-- Otto initialization
    void Init(int right_pitch, int right_roll, int left_pitch, int left_roll, int body, int head);
    //-- Attach & detach functions
    void AttachServos();
    void DetachServos();

    //-- Oscillator Trims
    void SetTrims(int right_pitch, int right_roll, int left_pitch, int left_roll, int body,
                  int head);

    //-- Predetermined Motion Functions
    void MoveServos(int time, int servo_target[]);
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                         double phase_diff[SERVO_COUNT], float cycle);

    //-- HOME = Otto at rest position
    void Home(bool hands_down = true);
    bool GetRestState();
    void SetRestState(bool state);

    // -- 手部动作
    void HandAction(int action, int times = 1, int amount = 30, int period = 1000);
    // action: 1=举左手, 2=举右手, 3=举双手, 4=放左手, 5=放右手, 6=放双手, 7=挥左手, 8=挥右手,
    // 9=挥双手, 10=拍打左手, 11=拍打右手, 12=拍打双手

    //-- 身体动作
    void BodyAction(int action, int times = 1, int amount = 30, int period = 1000);
    // action: 1=左转, 2=右转

    //-- 头部动作
    void HeadAction(int action, int times = 1, int amount = 10, int period = 500);
    // action: 1=抬头, 2=低头, 3=点头, 4=回中心, 5=连续点头

private:
    Oscillator servo_[SERVO_COUNT];

    int servo_pins_[SERVO_COUNT];
    int servo_trim_[SERVO_COUNT];
    int servo_initial_[SERVO_COUNT] = {180, 180, 0, 0, 90, 90};

    unsigned long final_time_;
    unsigned long partial_time_;
    float increment_[SERVO_COUNT];

    bool is_otto_resting_;

    void Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                 double phase_diff[SERVO_COUNT], float steps);
};

#endif  // __MOVEMENTS_H__