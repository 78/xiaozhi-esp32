#ifndef __POWERMANAGER_H__
#define __POWERMANAGER_H__

#include <functional>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

class PowerManager{
public:
    PowerManager();
    void Initialize();
    bool IsCharging();
    bool IsDischarging();
    bool IsChargingDone();
    int GetBatteryLevel();
    void CheckStartup();
    void Start5V();
    void Shutdown5V();
    void Start4G();
    void Shutdown4G();
    void Enable4G();
    void Disable4G();
    void Sleep();
    void CheckBatteryStatus();
    void OnChargingStatusChanged(std::function<void(bool)> callback);
    void OnChargingStatusDisChanged(std::function<void(bool)> callback);
    void OnBtLinkStatusChanged(std::function<void(bool)> callback);
    void InitializeBtModul();
    void DeinitBtModul();
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> charging_callback_;
    std::function<void(bool)> discharging_callback_;
    std::function<void(bool)> bt_link_callback_;
    int is_charging_ = -1;
    int is_discharging_ = -1;
    int call_count_ = 0;
    TaskHandle_t m_bt_task_handle;
    static void BtTask(void *arg);
};

#endif