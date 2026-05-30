#ifndef APP_SERVO_H
#define APP_SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ================= 硬件定义 ================= */
// 根据实际电路修改引脚
#define SERVO_PIN_PWR_CTRL  GPIO_NUM_10    // 舵机电源控制脚
#define SERVO_PIN_LEFT      GPIO_NUM_32    // 左舵机信号脚
#define SERVO_PIN_RIGHT     GPIO_NUM_11    // 右舵机信号脚

// 舵机通道枚举
typedef enum {
    SERVO_LEFT = 0,
    SERVO_RIGHT,
    SERVO_MAX_COUNT, // 计数用
    SERVO_BOTH       // 特殊指令用
} servo_id_t;

typedef enum {
    SERVO_TARGET_LEFT = SERVO_LEFT,
    SERVO_TARGET_RIGHT = SERVO_RIGHT,
    SERVO_TARGET_BOTH,       
} servo_target_t;

/* ================= 核心接口 ================= */

/**
 * @brief 初始化舵机子系统
 * 配置LEDC定时器、GPIO、安装Fade服务、创建管理任务
 */
void app_servo_init(void);

/**
 * @brief 设置微调值 (用于修正机械安装误差)
 * @param id 舵机ID
 * @param trim_angle 角度偏移量 (如 -5.0 ~ 5.0)
 */
void app_servo_set_trim(servo_id_t id, float trim_angle);

/**
 * @brief 控制单个舵机
 * CPU零负载：使用硬件定时器进行动作，任务此时处于阻塞状态
 * @param id 舵机ID
 * @param angle 目标角度 (0.0 ~ 180.0)
 * @param duration_ms 运动耗时 (0=最快速度, >0=硬件平滑插值)
 */
void app_servo_set_angle(servo_id_t id, float angle, uint32_t duration_ms); 

/**
 * @brief 同步控制双舵机
 * 启动几乎完全同步(微秒级差异)
 * @param angle_left 左舵机角度
 * @param angle_right 右舵机角度
 * @param duration_ms 运动耗时
 */
void app_servo_move_sync(float angle_left, float angle_right, uint32_t duration_ms);

void app_servo_set_target_angle(servo_target_t target, float angle, uint32_t duration_ms);

void app_servo_set_invert(bool inv_l, bool inv_r);

/**
 * @brief 获取舵机当前角度（用户侧逻辑角度，去除 trim/映射/invert 影响）
 * @param id 舵机ID (SERVO_LEFT 或 SERVO_RIGHT)
 * @return 当前角度 (0.0 ~ 180.0)，出错返回 -1.0f
 */
float app_servo_get_angle(servo_id_t id);

/**
 * @brief 从 NVS 加载舵机 trim，并立即应用。
 *        在 app_servo_init() 内部调用，也可在运行时重新加载。
 */
void app_servo_load_trim_from_nvs(void);

#ifdef __cplusplus
}
#endif

#endif // APP_SERVO_H