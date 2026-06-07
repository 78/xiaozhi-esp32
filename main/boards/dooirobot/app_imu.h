/**
 * @file    app_imu.h
 * @brief   IMU 应用层 — 偏航角积分 + 倾斜检测 + 推动检测
 */

#ifndef APP_IMU_H
#define APP_IMU_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_INT_PIN                 GPIO_NUM_19
#define IMU_TASK_PRIORITY           3
#define IMU_TASK_STACK              4096

/* ── 倾斜方向 ── */
typedef enum {
    IMU_TILT_NONE = 0,
    IMU_TILT_LEFT,
    IMU_TILT_RIGHT,
    IMU_TILT_FRONT,
    IMU_TILT_BACK,
    IMU_ENV_PUSH,
} imu_tilt_dir_t;

typedef struct {
    float ax, ay, az; // 单位: g
    float gx, gy, gz; // 单位: dps
} app_imu_data_t;

/* ── 回调类型 ── */
typedef void (*imu_tilt_cb_t)(imu_tilt_dir_t dir, void *arg);
typedef void (*imu_push_cb_t)(void *arg);  // <== 新增：推动回调

/**
 * @brief 初始化 IMU 应用层
 */
esp_err_t app_imu_init(void);

/**
 * @brief 注册倾斜回调
 */
void app_imu_set_tilt_callback(imu_tilt_cb_t cb, void *arg);

/**
 * @brief 使能/关闭倾斜检测
 */
void app_imu_enable_tilt_detect(bool enable, float threshold_deg);

/**
 * @brief 注册推动检测回调 (新增)
 * @param cb   回调函数（NULL = 清除）
 * @param arg  透传给回调的用户参数
 */
void app_imu_set_push_callback(imu_push_cb_t cb, void *arg);

/**
 * @brief 使能/关闭推动检测 (新增)
 * @param enable        是否开启
 * @param threshold_g   触发阈值（单位：g），推荐值 0.2f ~ 0.5f，越小越灵敏
 */
void app_imu_enable_push_detect(bool enable, float threshold_g);

/**
 * @brief 请求/释放 IMU 硬件
 */
void app_imu_require(bool require);

/** @brief 偏航角清零 */
void app_imu_reset_yaw(void);

/** @brief 获取当前偏航角（度） */
float app_imu_get_yaw(void);

/**
 * @brief 获取最新的 IMU 物理数据
 */
void app_imu_get_data(app_imu_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* APP_IMU_H */