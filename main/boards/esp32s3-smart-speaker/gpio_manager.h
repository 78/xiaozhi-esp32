#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include "config.h"
#include <driver/gpio.h>

class GpioManager {
public:
    static GpioManager& GetInstance();
    
    // 初始化GPIO系统
    bool Initialize();
    
    // GPIO控制方法
    void SetLedRing(bool state);
    void SetStatusLed(bool state);
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    GpioManager() = default;
    ~GpioManager() = default;
    GpioManager(const GpioManager&) = delete;
    GpioManager& operator=(const GpioManager&) = delete;
    
    bool initialized_ = false;
};

#endif // GPIO_MANAGER_H
