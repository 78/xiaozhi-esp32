#ifndef RTC_PCF8563_H
#define RTC_PCF8563_H

#include <ctime>
#include <functional>
#include <atomic>

#include <driver/gpio.h>
#include <driver/i2c_master.h>

#include "i2c_device.h"

class RtcPcf8563 : public I2cDevice {
public:
    RtcPcf8563(i2c_master_bus_handle_t i2c_bus, uint8_t addr);

    bool Init(gpio_num_t int_gpio = GPIO_NUM_NC);
    bool SetTime(const tm& local_tm);
    bool GetTime(tm& out_local_tm);

    bool SetAlarm(const tm& target_local_tm);
    bool DisableAlarm();
    bool ClearAlarmFlag();
    bool IsAlarmFired();

    bool StartCountdownTimer(uint8_t seconds);
    bool StopCountdownTimer();
    bool ClearTimerFlag();
    bool IsTimerFired();

    bool ResetI2cBus(const char* reason);

    void OnInterrupt(std::function<void()> callback);
    void NotifyFromIsr();
    bool ConsumeInterrupt();

private:
    static uint8_t ToBcd(int value);
    static int FromBcd(uint8_t value);
    bool EnableInterrupt(bool enable);

    gpio_num_t int_gpio_ = GPIO_NUM_NC;
    std::function<void()> callback_;
    std::atomic<bool> interrupt_pending_{false};
};

#endif // RTC_PCF8563_H
