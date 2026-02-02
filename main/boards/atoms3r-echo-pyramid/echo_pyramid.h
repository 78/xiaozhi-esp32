#ifndef ECHO_PYRAMID_H
#define ECHO_PYRAMID_H

#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <vector>
#include <atomic>


#define ECHO_PYRAMID_DEVICE_ADDR (0x1A)

// 触摸事件类型
enum class TouchEvent {
    LEFT_SLIDE_UP,    // Touch 1→2 滑动
    LEFT_SLIDE_DOWN,  // Touch 2→1 滑动
    RIGHT_SLIDE_UP,   // Touch 3→4 滑动
    RIGHT_SLIDE_DOWN, // Touch 4→3 滑动
};

// 灯效模式
enum class LightMode {
    OFF = 0,           // 关闭
    BREATHE = 1,       // 呼吸灯
    RAINBOW = 2,       // 彩虹效果
    CHASE = 3,         // 追逐效果
    STATIC = 4,        // 静态颜色
};

// 触摸事件回调类型
using TouchEventCallback = std::function<void(TouchEvent)>;

/**
 * Echo Pyramid 底座控制类
 * 负责 RGB 灯带和触摸按键的控制
 */
class EchoPyramid {
public:
    EchoPyramid(i2c_master_bus_handle_t i2c_bus, uint8_t echo_pyramid_addr = ECHO_PYRAMID_DEVICE_ADDR);
    ~EchoPyramid();

    // 触摸事件 API
    void addTouchEventCallback(TouchEventCallback callback);
    void clearTouchEventCallbacks();

    // 灯效 API
    void setLightMode(LightMode mode);
    LightMode getLightMode() const;
    void setLightColor(uint32_t color);
    void setLightBrightness(uint8_t strip, uint8_t brightness);

    // 触摸检测控制
    void startTouchDetection();
    void stopTouchDetection();

    // 暂停/恢复触摸事件回调（用于启动教程等场景）
    void pauseTouchCallbacks();
    void resumeTouchCallbacks();
    bool isTouchCallbacksPaused() const;

private:
    class Stm32Impl;
    Stm32Impl* stm32_ = nullptr;

    TaskHandle_t touch_task_handle_ = nullptr;
    std::vector<TouchEventCallback> touch_callbacks_;
    std::atomic<bool> touch_callbacks_paused_{false};

    // 触摸状态
    uint8_t touch_last_state_[4] = {0, 0, 0, 0};
    uint8_t touch_swipe_first_[2] = {0, 0};
    uint32_t touch_swipe_time_[2] = {0, 0};
    static constexpr uint32_t TOUCH_SWIPE_TIMEOUT_MS = 500;

    // 触摸任务
    static void TouchTask(void* arg);
    void ProcessSwipe(bool touch1, bool touch2, uint8_t& last_state1, uint8_t& last_state2,
                      uint8_t swipe_index, uint8_t touch_num1, uint8_t touch_num2, uint32_t current_time);
    void NotifyTouchEvent(TouchEvent event);
};

/**
 * Si5351 Clock Generator
 * I2C Address: 0x60
 */
class Si5351 : public I2cDevice {
public:
    Si5351(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x60);
};

/**
 * AW87559 Audio Amplifier
 * I2C Address: 0x5B
 */
class Aw87559 : public I2cDevice {
public:
    Aw87559(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x5B);
    void SetSpeaker(bool enable);
};

#endif // ECHO_PYRAMID_H

