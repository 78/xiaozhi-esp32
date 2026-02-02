#ifndef _GENU_MOVEMENTS_H_
#define _GENU_MOVEMENTS_H_

#include "oscillator.h"
#include <driver/ledc.h>

#define SERVO_COUNT 3

enum ServoChannel {
    HEAD = 0,
    LEFT_ARM = 1,
    RIGHT_ARM = 2
};

class GenuRobot {
public:
    GenuRobot();
    ~GenuRobot();

    void Init(int head_pin, int left_arm_pin, int right_arm_pin);
    void AttachServos();
    void DetachServos();
    
    // Core Movement
    void MoveServos(int time, int servo_target[]);
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period, double phase_diff[SERVO_COUNT], float cycle);
    
    // Behaviors
    void Home();
    void Happy();
    void Sad();
    void Angry();
    void Wave(); // Hello
    void Dance();
    void Comfort(); // Hug/Sooth
    void Excited();
    void Shy();
    void Sleepy();

private:
    Oscillator servo_[SERVO_COUNT];
    int servo_pins_[SERVO_COUNT];
    int servo_trim_[SERVO_COUNT];
    bool is_resting_ = false;

    // Movement helpers
    unsigned long final_time_;
    double increment_[SERVO_COUNT];
};

#endif // _GENU_MOVEMENTS_H_
