#include "app_motor.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#if ENABLE_MOTOR_IMU_CONTROL
#include "app_imu.h"
#endif

#if ENABLE_MOTOR_IR_POWER
#include "ir.h"
#include "app_ui_logic.h"
#endif

static const char* TAG = "APP_MOTOR";

/* ================= PWM 硬件配置 ================= */
#define MOTOR_LEDC_TIMER LEDC_TIMER_2
#define MOTOR_LEDC_MODE LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_RESOLUTION LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY 1023

#define MOTOR_CH_L_A LEDC_CHANNEL_6
#define MOTOR_CH_L_B LEDC_CHANNEL_1
#define MOTOR_CH_R_A LEDC_CHANNEL_2
#define MOTOR_CH_R_B LEDC_CHANNEL_3

/* ================= 内部状态定义 ================= */
typedef enum { MOTOR_STATE_STANDBY, MOTOR_STATE_ACTIVE, MOTOR_STATE_ESTOP } motor_state_t;

typedef enum {
    MOTOR_EXEC_IDLE,
    MOTOR_EXEC_REMOTE,
    MOTOR_EXEC_AUTO,
#if ENABLE_MOTOR_IMU_CONTROL
    MOTOR_EXEC_TURN_ANGLE
#endif
} motor_exec_mode_t;

typedef struct {
    QueueHandle_t cmd_queue;

    motor_state_t state;
    motor_exec_mode_t exec_mode;
    volatile bool is_active;

    float current_speed_l;
    float current_speed_r;

    float remote_target_l;
    float remote_target_r;
    int64_t last_remote_time_us;

    /* auto 模式：以 RAMP_STEP 斜坡逼近目标速度，跑满 duration_ms 后结束 */
    struct {
        float target_l, target_r;
        int64_t start_time_us;
        uint32_t duration_ms;
    } auto_params;

#if ENABLE_MOTOR_IMU_CONTROL
    struct {
        float target_angle;
        int16_t max_speed;
        bool is_imu_required;
        int64_t start_time_us;
    } turn_params;
#endif

    bool inv_l;
    bool inv_r;

} motor_context_t;

static motor_context_t g_motor_ctx = {0};

/* ================= IR 电源控制辅助函数 ================= */
static inline void motor_ir_on(void)
{
#if ENABLE_MOTOR_IR_POWER
    app_event_t evt = {.type = APP_EVT_IR_ENABLE};
    app_ui_logic_post(&evt, false);
#endif
}

static inline void motor_ir_off(void)
{
#if ENABLE_MOTOR_IR_POWER
    app_event_t evt = {.type = APP_EVT_IR_DISABLE};
    app_ui_logic_post(&evt, false);
#endif
}

/* ================= 硬件操作函数 ================= */
static inline int64_t motor_get_time_us(void) { return esp_timer_get_time(); }

static esp_err_t motor_hw_init(void) {
    ledc_timer_config_t timer_cfg = {.speed_mode = MOTOR_LEDC_MODE,
                                     .timer_num = MOTOR_LEDC_TIMER,
                                     .duty_resolution = MOTOR_LEDC_RESOLUTION,
                                     .freq_hz = APP_MOTOR_PWM_FREQ,
                                     .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_configs[] = {
        {.channel = MOTOR_CH_L_A, .gpio_num = APP_MOTOR_PIN_L_A,
         .speed_mode = MOTOR_LEDC_MODE, .timer_sel = MOTOR_LEDC_TIMER, .duty = 0, .hpoint = 0},
        {.channel = MOTOR_CH_L_B, .gpio_num = APP_MOTOR_PIN_L_B,
         .speed_mode = MOTOR_LEDC_MODE, .timer_sel = MOTOR_LEDC_TIMER, .duty = 0, .hpoint = 0},
        {.channel = MOTOR_CH_R_A, .gpio_num = APP_MOTOR_PIN_R_A,
         .speed_mode = MOTOR_LEDC_MODE, .timer_sel = MOTOR_LEDC_TIMER, .duty = 0, .hpoint = 0},
        {.channel = MOTOR_CH_R_B, .gpio_num = APP_MOTOR_PIN_R_B,
         .speed_mode = MOTOR_LEDC_MODE, .timer_sel = MOTOR_LEDC_TIMER, .duty = 0, .hpoint = 0}
    };
    for (int i = 0; i < 4; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&ch_configs[i]));
    }
    return ESP_OK;
}

static void motor_hw_set_pwm(int16_t speed, ledc_channel_t ch_a, ledc_channel_t ch_b) {
    if (speed > APP_MOTOR_MAX_SPEED)  speed = APP_MOTOR_MAX_SPEED;
    if (speed < -APP_MOTOR_MAX_SPEED) speed = -APP_MOTOR_MAX_SPEED;
    uint32_t duty = (uint32_t)(abs(speed) * MOTOR_PWM_MAX_DUTY / APP_MOTOR_MAX_SPEED);
    if (duty > 0) duty = 614 + duty * (MOTOR_PWM_MAX_DUTY - 614) / MOTOR_PWM_MAX_DUTY;
    if (speed >= 0) {
        ledc_set_duty(MOTOR_LEDC_MODE, ch_a, duty);
        ledc_set_duty(MOTOR_LEDC_MODE, ch_b, 0);
    } else {
        ledc_set_duty(MOTOR_LEDC_MODE, ch_a, 0);
        ledc_set_duty(MOTOR_LEDC_MODE, ch_b, duty);
    }
    ledc_update_duty(MOTOR_LEDC_MODE, ch_a);
    ledc_update_duty(MOTOR_LEDC_MODE, ch_b);
}
static void motor_hw_update_output(void) {
    float final_l = g_motor_ctx.current_speed_l * (g_motor_ctx.inv_l ? -1.0f : 1.0f);
    float final_r = g_motor_ctx.current_speed_r * (g_motor_ctx.inv_r ? -1.0f : 1.0f);

    motor_hw_set_pwm((int16_t)final_l,  MOTOR_CH_L_A, MOTOR_CH_L_B);
    motor_hw_set_pwm((int16_t)-final_r, MOTOR_CH_R_A, MOTOR_CH_R_B); 
}

static void motor_hw_stop(void) {
    g_motor_ctx.current_speed_l = 0;
    g_motor_ctx.current_speed_r = 0;
    g_motor_ctx.remote_target_l = 0;
    g_motor_ctx.remote_target_r = 0;
    motor_hw_update_output();
}

static float motor_approach(float current, float target, float step) {
    if (current < target) {
        current += step;
        if (current > target) current = target;
    } else if (current > target) {
        current -= step;
        if (current < target) current = target;
    }
    return current;
}

static void motor_mix_arcade(float throttle, float steering, float* out_l, float* out_r) {
    if (throttle >  1.0f) throttle =  1.0f;
    if (throttle < -1.0f) throttle = -1.0f;
    if (steering >  1.0f) steering =  1.0f;
    if (steering < -1.0f) steering = -1.0f;
    float left  = throttle + steering;
    float right = throttle - steering;
    float max_val = fmaxf(fabsf(left), fabsf(right));
    if (max_val > 1.0f) { left /= max_val; right /= max_val; }
    *out_l = left  * APP_MOTOR_MAX_SPEED;
    *out_r = right * APP_MOTOR_MAX_SPEED;
}

static void motor_cleanup_previous_mode(void) {
#if ENABLE_MOTOR_IMU_CONTROL
    if (g_motor_ctx.exec_mode == MOTOR_EXEC_TURN_ANGLE &&
        g_motor_ctx.turn_params.is_imu_required) {
        app_imu_require(false);
        g_motor_ctx.turn_params.is_imu_required = false;
    }
#endif
}

static void motor_exec_remote_tick(void) {
    int64_t now = motor_get_time_us();
    if ((now - g_motor_ctx.last_remote_time_us) > (APP_MOTOR_CMD_TIMEOUT_MS * 1000)) {
        g_motor_ctx.remote_target_l = 0;
        g_motor_ctx.remote_target_r = 0;
    }
    g_motor_ctx.current_speed_l = motor_approach(
        g_motor_ctx.current_speed_l, g_motor_ctx.remote_target_l, APP_MOTOR_RAMP_STEP);
    g_motor_ctx.current_speed_r = motor_approach(
        g_motor_ctx.current_speed_r, g_motor_ctx.remote_target_r, APP_MOTOR_RAMP_STEP);
    motor_hw_update_output();
}

/**
 * @brief auto 模式 tick
 * @return true  duration 到期，切回 REMOTE/IDLE
 * @return false 继续执行
 */
static bool motor_exec_auto_tick(void) {
    g_motor_ctx.current_speed_l = motor_approach(
        g_motor_ctx.current_speed_l,
        g_motor_ctx.auto_params.target_l,
        APP_MOTOR_RAMP_STEP);
    g_motor_ctx.current_speed_r = motor_approach(
        g_motor_ctx.current_speed_r,
        g_motor_ctx.auto_params.target_r,
        APP_MOTOR_RAMP_STEP);
    motor_hw_update_output();

    int64_t elapsed_ms =
        (motor_get_time_us() - g_motor_ctx.auto_params.start_time_us) / 1000;
    return (elapsed_ms >= (int64_t)g_motor_ctx.auto_params.duration_ms);
}

#if ENABLE_MOTOR_IMU_CONTROL
/**
 * @brief 转角控制 tick
 *
 * 约定：error > 0 → 右转（右轮前进、左轮后退）
 *       error < 0 → 左转（左轮前进、右轮后退）
 *
 * 偏航角正方向需与此一致：车体右转时 app_imu_get_yaw() 递增。
 * 如果实测右转时 yaw 递减，请在 app_imu.c 中将 gz = -phys->gz 改为 gz = +phys->gz。
 *
 * @return true  转向完成或超时
 * @return false 继续执行
 */
static bool motor_exec_turn_angle_tick(void) {
    float current_yaw = app_imu_get_yaw();
    float error       = g_motor_ctx.turn_params.target_angle - current_yaw;
    int64_t now       = motor_get_time_us();

    /* 到位判定：误差 < 2° 或超时 10s */
    if (fabsf(error) < 2.0f ||
        (now - g_motor_ctx.turn_params.start_time_us) > 10000000LL)
    {
        g_motor_ctx.current_speed_l = 0;
        g_motor_ctx.current_speed_r = 0;
        motor_hw_update_output();
        // ESP_LOGI(TAG, "Turn done. Final Yaw: %.1f, Error: %.1f", current_yaw, error);
        return true;
    }

    /*
     * FIX #4: 用 fabsf 取绝对值，防止 max_speed 传入负数时
     *         min_motor_power 保护失效，导致方向反转
     */
    float output_speed = fabsf((float)g_motor_ctx.turn_params.max_speed);
    if (fabsf(error) < 15.0f) output_speed *= 0.6f;

    const float min_motor_power = 40.0f;
    if (output_speed < min_motor_power) output_speed = min_motor_power;

    /*
     * error > 0 → 目标角在当前偏航右侧 → 右转
     *   右转：左轮正转、右轮反转
     *   current_speed_l = +output_speed
     *   current_speed_r = -output_speed
     *
     * error < 0 → 目标角在当前偏航左侧 → 左转
     *   左转：左轮反转、右轮正转
     *   current_speed_l = -output_speed
     *   current_speed_r = +output_speed
     *
     * 注意：motor_hw_update_output 对右轮输出取反（-final_r），
     *       所以 current_speed_r > 0 → 右轮实际前进。
     */
    if (error < 0) {
        g_motor_ctx.current_speed_l =  output_speed;
        g_motor_ctx.current_speed_r = -output_speed; // 此时硬件层会把 -speed 变成 +PWM(向后)
    } else {
        g_motor_ctx.current_speed_l = -output_speed;
        g_motor_ctx.current_speed_r =  output_speed;
    }
    motor_hw_update_output();
    return false;
}
#endif

static bool motor_process_cmd(const app_motor_cmd_t* cmd) {
    switch (cmd->type) {
        case APP_MOTOR_CMD_REMOTE: {
            motor_cleanup_previous_mode();
            float target_l, target_r;
            motor_mix_arcade(cmd->params.remote.throttle, cmd->params.remote.steering,
                             &target_l, &target_r);
            g_motor_ctx.remote_target_l     = target_l;
            g_motor_ctx.remote_target_r     = target_r;
            g_motor_ctx.last_remote_time_us = motor_get_time_us();
            g_motor_ctx.exec_mode = MOTOR_EXEC_REMOTE;
            return true;
        }
        case APP_MOTOR_CMD_AUTO: {
            motor_cleanup_previous_mode();
            g_motor_ctx.auto_params.target_l    = cmd->params.auto_move.speed_l;
            g_motor_ctx.auto_params.target_r    = cmd->params.auto_move.speed_r;
            g_motor_ctx.auto_params.duration_ms = cmd->params.auto_move.duration_ms;
            g_motor_ctx.auto_params.start_time_us = motor_get_time_us();
            g_motor_ctx.exec_mode = MOTOR_EXEC_AUTO;
            return true;
        }
        case APP_MOTOR_CMD_ESTOP:
            motor_cleanup_previous_mode();
            g_motor_ctx.state = MOTOR_STATE_ESTOP;
            motor_hw_stop();
            motor_ir_off();
            return false;
        case APP_MOTOR_CMD_RESUME:
            if (g_motor_ctx.state == MOTOR_STATE_ESTOP) {
                g_motor_ctx.state = MOTOR_STATE_STANDBY;
            }
            return false;
        case APP_MOTOR_CMD_STOP:
            motor_cleanup_previous_mode();
            g_motor_ctx.remote_target_l = 0;
            g_motor_ctx.remote_target_r = 0;
            g_motor_ctx.exec_mode = MOTOR_EXEC_REMOTE;
            return true;
#if ENABLE_MOTOR_IMU_CONTROL
        case APP_MOTOR_CMD_TURN_ANGLE: {
            motor_cleanup_previous_mode();
            g_motor_ctx.turn_params.target_angle = cmd->params.turn_angle.target_angle;
            g_motor_ctx.turn_params.max_speed    = cmd->params.turn_angle.max_speed;
            if (g_motor_ctx.turn_params.max_speed < 10)
                g_motor_ctx.turn_params.max_speed = 10;

            /*
             * FIX #5: 先 require(true)（内部会清零 yaw_integral），
             *         再 reset_yaw()（再次清零 + 清零 last_ts_us），
             *         最后记录 start_time，顺序严格保证初始值干净。
             */
            app_imu_require(true);
            app_imu_reset_yaw();

            g_motor_ctx.turn_params.is_imu_required = true;
            g_motor_ctx.turn_params.start_time_us   = motor_get_time_us();
            g_motor_ctx.exec_mode = MOTOR_EXEC_TURN_ANGLE;
            return true;
        }
#endif
        default:
            return false;
    }
}

static void motor_task(void* arg) {
    app_motor_cmd_t cmd;
    const TickType_t ctrl_period = pdMS_TO_TICKS(1000 / APP_MOTOR_CTRL_FREQ_HZ);

    vTaskDelay(pdMS_TO_TICKS(1000));
    if (motor_hw_init() != ESP_OK) return;

    g_motor_ctx.cmd_queue = xQueueCreate(APP_MOTOR_QUEUE_SIZE, sizeof(app_motor_cmd_t));
    if (!g_motor_ctx.cmd_queue) return;

    while (1) {
        g_motor_ctx.is_active = false;
        g_motor_ctx.state     = MOTOR_STATE_STANDBY;
        motor_hw_stop();
        motor_ir_off();

        if (xQueueReceive(g_motor_ctx.cmd_queue, &cmd, portMAX_DELAY) != pdPASS)
            continue;
        if (cmd.type == APP_MOTOR_CMD_RESUME) continue;
        if (g_motor_ctx.state == MOTOR_STATE_ESTOP) continue;

        g_motor_ctx.is_active = true;
        g_motor_ctx.state     = MOTOR_STATE_ACTIVE;
        motor_ir_on();

        if (!motor_process_cmd(&cmd)) continue;

        TickType_t last_wake_time = xTaskGetTickCount();

        while (g_motor_ctx.state == MOTOR_STATE_ACTIVE) {
            switch (g_motor_ctx.exec_mode) {
                case MOTOR_EXEC_REMOTE:
                    motor_exec_remote_tick();
                    break;
                case MOTOR_EXEC_AUTO:
                    if (motor_exec_auto_tick()) {
                        g_motor_ctx.remote_target_l     = g_motor_ctx.current_speed_l;
                        g_motor_ctx.remote_target_r     = g_motor_ctx.current_speed_r;
                        g_motor_ctx.last_remote_time_us = motor_get_time_us();
                        g_motor_ctx.exec_mode = MOTOR_EXEC_REMOTE;
                    }
                    break;
#if ENABLE_MOTOR_IMU_CONTROL
                case MOTOR_EXEC_TURN_ANGLE:
                    if (motor_exec_turn_angle_tick()) {
                        motor_cleanup_previous_mode();
                        g_motor_ctx.remote_target_l = 0;
                        g_motor_ctx.remote_target_r = 0;
                        g_motor_ctx.exec_mode = MOTOR_EXEC_IDLE;
                    }
                    break;
#endif
                default:
                    break;
            }

            app_motor_cmd_t new_cmd;
            if (xQueueReceive(g_motor_ctx.cmd_queue, &new_cmd, 0) == pdPASS) {
                if (!motor_process_cmd(&new_cmd)) break;
            }

            bool should_idle =
                ((g_motor_ctx.exec_mode == MOTOR_EXEC_REMOTE) ||
                 (g_motor_ctx.exec_mode == MOTOR_EXEC_IDLE)) &&
                (fabsf(g_motor_ctx.current_speed_l) < 0.5f) &&
                (fabsf(g_motor_ctx.current_speed_r) < 0.5f) &&
                (fabsf(g_motor_ctx.remote_target_l) < 0.5f) &&
                (fabsf(g_motor_ctx.remote_target_r) < 0.5f);

            if (should_idle) {
                motor_ir_off();
                if (xQueueReceive(g_motor_ctx.cmd_queue, &new_cmd,
                                  pdMS_TO_TICKS(APP_MOTOR_IDLE_TIMEOUT_MS)) == pdPASS) {
                    motor_ir_on();
                    if (!motor_process_cmd(&new_cmd)) break;
                } else {
                    break;
                }
            }
            vTaskDelayUntil(&last_wake_time, ctrl_period);
        }
    }
}

esp_err_t app_motor_init(void) {
    bool dummy1, dummy2;

    if (xTaskCreatePinnedToCore(motor_task, "motor_task", 2048, NULL, 10, NULL, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_motor_set_remote(float throttle, float steering) {
    app_motor_cmd_t cmd = {
        .type = APP_MOTOR_CMD_REMOTE,
        .params.remote = {throttle, steering}
    };
    if (xQueueSend(g_motor_ctx.cmd_queue, &cmd, 0) != pdPASS) {
        xQueueReset(g_motor_ctx.cmd_queue);
        xQueueSend(g_motor_ctx.cmd_queue, &cmd, 0);
    }
    return ESP_OK;
}

esp_err_t app_motor_move_auto(int16_t speed_l, int16_t speed_r, uint32_t duration_ms) {
    app_motor_cmd_t cmd = {
        .type = APP_MOTOR_CMD_AUTO,
        .params.auto_move = {speed_l, speed_r, duration_ms}
    };
    if (xQueueSend(g_motor_ctx.cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdPASS)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

#if ENABLE_MOTOR_IMU_CONTROL
esp_err_t app_motor_turn_angle(float target_angle_deg, int16_t max_speed) {
    app_motor_cmd_t cmd = {
        .type = APP_MOTOR_CMD_TURN_ANGLE,
        .params.turn_angle = {.target_angle = target_angle_deg, .max_speed = max_speed}
    };
    if (xQueueSend(g_motor_ctx.cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdPASS)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}
#endif

esp_err_t app_motor_estop(void) {
    app_motor_cmd_t cmd = {.type = APP_MOTOR_CMD_ESTOP};
    xQueueReset(g_motor_ctx.cmd_queue);
    xQueueSend(g_motor_ctx.cmd_queue, &cmd, 0);
    return ESP_OK;
}

esp_err_t app_motor_resume(void) {
    app_motor_cmd_t cmd = {.type = APP_MOTOR_CMD_RESUME};
    if (xQueueSend(g_motor_ctx.cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdPASS)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

esp_err_t app_motor_stop(void) {
    app_motor_cmd_t cmd = {.type = APP_MOTOR_CMD_STOP};
    if (xQueueSend(g_motor_ctx.cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdPASS)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

bool app_motor_is_active(void) { return g_motor_ctx.is_active; }

void app_motor_set_invert(bool inv_l, bool inv_r) {
    g_motor_ctx.inv_l = inv_l;
    g_motor_ctx.inv_r = inv_r;
}