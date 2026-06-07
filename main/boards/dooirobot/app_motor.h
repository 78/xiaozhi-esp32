#ifndef APP_MOTOR_H 
#define APP_MOTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 宏定义：是否开启电机角度驱动功能 */
#ifndef ENABLE_MOTOR_IMU_CONTROL
#define ENABLE_MOTOR_IMU_CONTROL 1
#endif

/**
 * 宏定义：是否开启 IR 电源联动控制
 *
 * 依赖：需要在项目中包含 ir.h / ir.c，并已完成 ir_init() 初始化。
 * 若不需要此功能，将下方的 1 改为 0 即可。
 */
#ifndef ENABLE_MOTOR_IR_POWER
#define ENABLE_MOTOR_IR_POWER 1
#endif

/* ================= 硬件引脚配置 ================= */
#define APP_MOTOR_PIN_L_A GPIO_NUM_17
#define APP_MOTOR_PIN_L_B GPIO_NUM_18
#define APP_MOTOR_PIN_R_A GPIO_NUM_26
#define APP_MOTOR_PIN_R_B GPIO_NUM_27

/* ================= 参数配置 ================= */
#define APP_MOTOR_MAX_SPEED 100         // 逻辑最大速度 (-100 ~ 100)
#define APP_MOTOR_PWM_FREQ 20000        // PWM 频率 20kHz
#define APP_MOTOR_CTRL_FREQ_HZ 50       // 控制回路频率 50Hz (20ms 周期)
#define APP_MOTOR_RAMP_STEP 30          // 每周期最大速度变化量
#define APP_MOTOR_CMD_TIMEOUT_MS 500    // 遥控模式指令超时时间 (ms)
#define APP_MOTOR_IDLE_TIMEOUT_MS 3000  // 空闲超时时间，超时后进入待机 (ms)
#define APP_MOTOR_QUEUE_SIZE 8          // 消息队列深度

/* ================= 指令类型定义 ================= */
typedef enum {
    APP_MOTOR_CMD_REMOTE,  // 遥控模式指令 (油门+转向)
    APP_MOTOR_CMD_AUTO,    // 自动模式指令 (定时定速)
    APP_MOTOR_CMD_ESTOP,   // 紧急停止
    APP_MOTOR_CMD_RESUME,  // 恢复运行
    APP_MOTOR_CMD_STOP,    // 正常停止
#if ENABLE_MOTOR_IMU_CONTROL
    APP_MOTOR_CMD_TURN_ANGLE  // 角度转向指令
#endif
} app_motor_cmd_type_t;

typedef struct {
    app_motor_cmd_type_t type;

    union {
        // 遥控模式参数
        struct {
            float throttle;  // 油门 (-1.0 ~ 1.0)
            float steering;  // 转向 (-1.0 ~ 1.0)
        } remote;

        // 自动模式参数
        struct {
            int16_t speed_l;       // 左轮速度 (-100 ~ 100)
            int16_t speed_r;       // 右轮速度 (-100 ~ 100)
            uint32_t duration_ms;  // 持续时间 (ms)
        } auto_move;

#if ENABLE_MOTOR_IMU_CONTROL
        // 角度转向参数
        struct {
            float target_angle;  // 目标角度增量，单位：度 (+90 或 -90 等)
            int16_t max_speed;   // 最大旋转速度 (1 ~ 100)
        } turn_angle;
#endif
    } params;

} app_motor_cmd_t;

/* ================= API 接口 ================= */
esp_err_t app_motor_init(void);
esp_err_t app_motor_set_remote(float throttle, float steering);
esp_err_t app_motor_move_auto(int16_t speed_l, int16_t speed_r, uint32_t duration_ms);

#if ENABLE_MOTOR_IMU_CONTROL
/**
 * @brief [角度控制模式] 使用IMU闭环控制旋转指定角度
 * @param target_angle_deg 相对旋转角度 (度)，正数为右转/左转
 * @param max_speed 旋转最大速度 (1~100)
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t app_motor_turn_angle(float target_angle_deg, int16_t max_speed);
#endif

esp_err_t app_motor_estop(void);
esp_err_t app_motor_resume(void);
esp_err_t app_motor_stop(void);
bool app_motor_is_active(void);
void app_motor_set_invert(bool inv_l, bool inv_r);

#ifdef __cplusplus
}
#endif

#endif  // APP_MOTOR_H