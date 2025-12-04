#ifndef _CST816X_H_
#define _CST816X_H_

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <sys/time.h>
#include <array>

#define ES8311_VOL_MIN 0
#define ES8311_VOL_MAX 100

enum class TouchEventType {
    SINGLE_CLICK,    // 单击事件
    DOUBLE_CLICK,    // 双击事件
    LONG_PRESS_START,// 长按开始事件
    LONG_PRESS_END   // 长按结束事件
};

struct TouchEvent {
    TouchEventType type;  
    int x;                
    int y;               
};

class Cst816x : public I2cDevice {
private:
    struct TouchPoint_t {
        int num = 0; 
        int x = -1;   
        int y = -1;   
    };

    struct TouchThresholdConfig {
        int x;                          // 目标X坐标
        int y;                          // 目标Y坐标
        int64_t single_click_thresh_us; // 单击最大时长（us）
        int64_t double_click_window_us; // 双击窗口（us）
        int64_t long_press_thresh_us;   // 长按阈值（us）
    };

    const TouchThresholdConfig DEFAULT_THRESHOLD = {
        .x = -1, .y = -1,                  
        .single_click_thresh_us = 120000,  // 150ms
        .double_click_window_us = 240000,  // 150ms
        .long_press_thresh_us = 4000000    // 4000ms
    };

    const std::array<TouchThresholdConfig, 3> TOUCH_THRESHOLD_TABLE = {
        {
            {20, 600, 200000, 240000, 2000000}, // 音量+
            {40, 600, 200000, 240000, 4000000}, // boot按键
            {60, 600, 200000, 240000, 2000000}  // 音量-
        }
    };

    const TouchThresholdConfig& getThresholdConfig(int x, int y);

    uint8_t* read_buffer_ = nullptr;  
    TouchPoint_t tp_;                 

    bool is_touching_ = false;              
    int64_t touch_start_time_ = 0;          // 触摸开始时间（us）
    int64_t last_release_time_ = 0;         // 上次释放时间（us）
    int click_count_ = 0;                   // 单击计数（双击检测用）
    bool long_press_started_ = false;       // 长按是否已触发

    bool is_volume_long_pressing_ = false;   // 是否处于音量长按调整中
    int volume_long_press_dir_ = 0;          // 调整方向：1=递增，-1=递减
    int64_t last_volume_adjust_time_ = 0;    // 上次调整音量的时间（us）
    const int64_t VOL_ADJ_INTERVAL_US = 200000; // 音量调整间隔（100ms）
    const int VOL_ADJ_STEP = 5;                // 每次调整步长

    int64_t getCurrentTimeUs();

public:
    Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    ~Cst816x();

    void InitCst816d();
    void UpdateTouchPoint();
    void resetTouchCounters();
    static void touchpad_daemon(void* param);
    
    const TouchPoint_t& GetTouchPoint() { return tp_; }
};

#endif