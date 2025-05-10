#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 定义LED灯带工作模式
typedef enum {
    WS2812_MODE_BLINK_BLUE = 0,  // 蓝色闪烁
    WS2812_MODE_BLINK_RED,       // 红色闪烁
    WS2812_MODE_BLINK_GREEN,     // 绿色闪烁
    WS2812_MODE_RAINBOW,        // 彩虹模式
    WS2812_MODE_CHASE,        // 跑马灯模式
    WS2812_MODE_SOLID,        // 全亮模式
    WS2812_MODE_DIM,          // 微光模式
    WS2812_MODE_OFF,          // 关闭模式
    WS2812_MODE_BREATHING,    // 呼吸灯模式
    WS2812_MODE_FIRE,         // 火焰效果
    WS2812_MODE_TWINKLE,      // 闪烁效果
    WS2812_MODE_WAVE,         // 波浪效果
    WS2812_MODE_ALTERNATE,    // 交替闪烁
    WS2812_MODE_PULSE,        // 脉冲效果
    WS2812_MODE_COMET,        // 彗星效果
    WS2812_MODE_RAINBOW_CYCLE, // 彩虹循环
    WS2812_MODE_RAINBOW_CHASE, // 彩虹追逐
    WS2812_MODE_RAINBOW_WAVE,  // 彩虹波浪
    WS2812_MODE_RAINBOW_FIRE,  // 彩虹火焰
    WS2812_MODE_RAINBOW_TWINKLE, // 彩虹闪烁
    WS2812_MODE_RAINBOW_BREATHING, // 彩虹呼吸
    WS2812_MODE_RAINBOW_PULSE, // 彩虹脉冲
    WS2812_MODE_RAINBOW_ALTERNATE, // 彩虹交替
    WS2812_MODE_RAINBOW_COMET, // 彩虹彗星
    WS2812_MODE_COLOR_WIPE,   // 颜色擦除
    WS2812_MODE_COLOR_CHASE,  // 颜色追逐
    WS2812_MODE_COLOR_WAVE,   // 颜色波浪
    WS2812_MODE_COLOR_FIRE,   // 颜色火焰
    WS2812_MODE_COLOR_TWINKLE, // 颜色闪烁
    WS2812_MODE_COLOR_BREATHING, // 颜色呼吸
    WS2812_MODE_COLOR_PULSE,  // 颜色脉冲
    WS2812_MODE_COLOR_ALTERNATE, // 颜色交替
    WS2812_MODE_COLOR_COMET,  // 颜色彗星
    WS2812_MODE_MAX           // 模式数量
} ws2812_mode_t;

typedef enum {
    WS2812_STATE_BOOTING = 0,
    WS2812_STATE_LISTENING_NO_VOICE,
    WS2812_STATE_LISTENING_VOICE,
    WS2812_STATE_SPEAKING,
    WS2812_STATE_IDLE,
    WS2812_STATE_MAX
} ws2812_state_t;

/**
 * @brief 设置WS2812 LED灯带的工作模式
 * 
 * @param mode 工作模式，详见ws2812_mode_t枚举定义
 */
extern "C" void ws2812_set_wave_mode(int mode);

/**
 * @brief 获取当前WS2812 LED灯带的工作模式
 * 
 * @return 当前工作模式
 */



extern "C" void ws2812_set_brightness(uint8_t brightness);

extern "C" uint8_t ws2812_get_brightness(void);

extern "C" void ws2812_turn_off(void);
extern "C" void ws2812_turn_on(void);
extern "C" void ws2812_set_mode(ws2812_mode_t mode);
extern "C" int  ws2812_get_wave_mode(void);
extern "C" void ws2812_set_state(ws2812_state_t state);
/**
 * @brief 启动WS2812测试任务
 * 
 * 此函数创建一个任务来控制WS2812 LED灯带，
 * 根据设置的模式显示不同的动画效果
 */
void ws2812_start(void);

#ifdef __cplusplus
}
#endif 