#ifndef __OSCILLATOR_H__
#define __OSCILLATOR_H__

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define M_PI 3.14159265358979323846

#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

#define SERVO_MIN_PULSEWIDTH_US 500           // 最小脉宽（微秒）
#define SERVO_MAX_PULSEWIDTH_US 2500          // 最大脉宽（微秒）
#define SERVO_MIN_DEGREE -90                  // 最小角度
#define SERVO_MAX_DEGREE 90                   // 最大角度
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD 20000           // 20000 ticks, 20ms

class Oscillator {
public:
    Oscillator(int trim = 0);
    ~Oscillator();
    void attach(int pin, bool rev = false);
    void detach();

    void SetA(unsigned int amplitude) { _amplitude = amplitude; };
    void SetO(int offset) { _offset = offset; };
    void SetPh(double Ph) { _phase0 = Ph; };
    void SetT(unsigned int period);
    void SetTrim(int trim) { _trim = trim; };
    void SetLimiter(int diff_limit) { _diff_limit = diff_limit; };
    void DisableLimiter() { _diff_limit = 0; };
    int getTrim() { return _trim; };
    void SetPosition(int position);
    void Stop() { _stop = true; };
    void Play() { _stop = false; };
    void Reset() { _phase = 0; };
    void refresh();
    int getPosition() { return _pos; }

private:
    bool next_sample();
    void write(int position);
    uint32_t angle_to_compare(int angle);

private:
    bool _is_attached;

    //-- Oscillators parameters
    unsigned int _amplitude;  //-- Amplitude (degrees)
    int _offset;              //-- Offset (degrees)
    unsigned int _period;     //-- Period (miliseconds)
    double _phase0;           //-- Phase (radians)

    //-- Internal variables
    int _pos;                      //-- Current servo pos
    int _pin;                      //-- Pin where the servo is connected
    int _trim;                     //-- Calibration offset
    double _phase;                 //-- Current phase
    double _inc;                   //-- Increment of phase
    double _numberSamples;         //-- Number of samples
    unsigned int _samplingPeriod;  //-- sampling period (ms)

    long _previousMillis;
    long _currentMillis;

    //-- Oscillation mode. If true, the servo is stopped
    bool _stop;

    //-- Reverse mode
    bool _rev;

    int _diff_limit;
    long _previousServoCommandMillis;

    ledc_channel_t _ledc_channel;
    ledc_mode_t _ledc_speed_mode;
};

#endif  // __OSCILLATOR_H__