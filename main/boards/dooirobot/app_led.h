#ifndef __APP_LED_H__
#define __APP_LED_H__

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 硬件配置 ================= */
#define LED_GPIO_LEFT       GPIO_NUM_39
#define LED_GPIO_RIGHT      GPIO_NUM_9

/* ================= 效果枚举 ================= */
typedef enum {
    LED_EFFECT_OFF = 0,
    LED_EFFECT_RGB,            // 固定颜色 (立即跳转)
    LED_EFFECT_BREATHING,      // 呼吸灯 (基于亮度Gamma校正)
    LED_EFFECT_RAINBOW,        // 彩虹流光
    LED_EFFECT_BLINK,          // 闪烁
    // 预留扩展: 比如 LED_EFFECT_FADE (平滑过渡到某色)
} led_effect_type_t;

/* ================= 外部接口 ================= */

/**
 * @brief 初始化LED子系统
 * 配置RMT驱动，创建FreeRTOS任务和队列
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t app_led_init(void);

/**
 * @brief 反初始化，释放资源
 */
void app_led_deinit(void);

/**
 * @brief 设置左右LED为固定颜色 (线程安全)
 * 格式: 0xRRGGBB (例如: 0xFF0000 红色)
 */
void app_led_set_rgb(uint32_t left_color, uint32_t right_color);

/**
 * @brief 设置呼吸效果
 * @param color       基准颜色
 * @param period_ms   周期(ms)，建议 > 1000ms
 * @param symmetric   true: 双灯同步, false: 仅左灯呼吸
 */
void app_led_set_breathing(uint32_t color, uint16_t period_ms, bool symmetric);

/**
 * @brief 设置彩虹效果
 * @param period_ms   颜色循环一周的时间
 * @param symmetric   true: 同步, false: 左右反相
 */
void app_led_set_rainbow(uint16_t period_ms, bool symmetric);

/**
 * @brief 设置闪烁效果
 * @param color       闪烁颜色
 * @param period_ms   闪烁周期
 * @param symmetric   true: 同步闪, false: 仅左灯闪
 */
void app_led_set_blink(uint32_t color, uint16_t period_ms, bool symmetric);

/**
 * @brief 关闭所有LED
 */
void app_led_off(void);

/**
 * @brief 随机效果测试 (用于演示)
 */
void app_led_random_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LED_H__ */