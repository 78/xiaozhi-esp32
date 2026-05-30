#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 充电状态变化回调
 * @param is_charging  true=正在充电, false=未充电
 */
typedef void (*power_charging_cb_t)(bool is_charging);

/**
 * 低电量状态变化回调
 * @param is_low_battery  true=进入低电量, false=脱离低电量
 */
typedef void (*power_low_battery_cb_t)(bool is_low_battery);

typedef struct {
    power_charging_cb_t on_charging_changed;
    power_low_battery_cb_t on_low_battery_changed;
} power_config_t;

/**
 * @brief 初始化电池管理模块
 */
esp_err_t power_manager_init(const power_config_t *config);

/**
 * @brief 获取当前电量百分比 (0~100)
 */
uint8_t power_manager_get_level(void);

/**
 * @brief 获取充电状态（电量满100%时也返回false）
 */
bool power_manager_is_charging(void);

/**
 * @brief 获取放电状态
 */
bool power_manager_is_discharging(void);

/**
 * @brief 获取低电量状态
 */
bool power_manager_is_low_battery(void);

/**
 * @brief 暂停采样（执行大电流操作时调用）
 */
void power_manager_pause(void);

/**
 * @brief 恢复采样
 */
void power_manager_resume(void);

/**
 * @brief 销毁模块，释放资源
 */
void power_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_H */
