#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"   // 互斥锁
#include <esp_timer.h>            // esp_timer_get_time()

static const char *TAG = "servo";

/* 保护舵机并发访问的互斥锁（前置声明，供 legs_move_smooth 等早期函数使用） */
static SemaphoreHandle_t s_servo_mutex = NULL;

/* 紧急停止标志（前置声明，legs_move_smooth 在函数定义之前需要访问） */
static volatile bool s_emergency_stop = false;

/* ================================================================
 * 内部工具函数
 * ================================================================ */

/* 角度 → PWM duty（参考 dog_control.cc 公式）
 * pulse_us = 500 + angle * 2000 / 180
 * duty = pulse_us / 20000 * 4095
 * 同时自动限幅 45~135°
 */
static uint32_t angle_to_duty_clamped(uint32_t angle)
{
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    /* 使用 64位乘法防止溢出 */
    uint32_t pulse_us = 500 + (angle * 2000U) / 180U;
    return (uint32_t)((uint64_t)pulse_us * SERVO_MAX_DUTY / 20000U);
}

/* 设置单个舵机 duty */
static void set_duty(ledc_channel_t ch, uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

/* 设置单个舵机角度（限幅保护） */
static void leg_set_angle(servo_leg_t *leg, uint32_t angle)
{
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;
    leg->angle = angle;
    set_duty(leg->ch, angle_to_duty_clamped(angle));
}

/**
 * @brief 同时平滑移动最多4条腿到目标角度
 *
 * @param legs       腿指针数组（可含 NULL，NULL 表示该位置腿不动）
 * @param targets    对应的目标角度数组（度）
 * @param n          数组长度（≤4）
 * @param total_ms   本拍总时长（ms），内部均分给 WALK_SMOOTH_STEPS 小步
 */
static void legs_move_smooth(servo_leg_t **legs, const int32_t *targets,
                             int n, int32_t total_ms)
{
    int steps = WALK_SMOOTH_STEPS;
    int32_t step_ms = total_ms / steps;
    if (step_ms < 1) step_ms = 1;

    int32_t starts[4] = {0};
    for (int i = 0; i < n; i++) {
        if (legs[i]) starts[i] = (int32_t)legs[i]->angle;
    }

    /* 微秒级精确延时基准：用 esp_timer_get_time() 替代 vTaskDelayUntil。
     * 每步计算目标结束时间戳，>5ms 用 vTaskDelay(1) 让出 CPU，
     * ≤5ms 进入 busy-wait 保证微秒级精度。 */
    int64_t step0_us = esp_timer_get_time();

    for (int s = 1; s <= steps; s++) {
        /* 急停检查：发现急停标志立即退出，不再继续移动 */
        if (s_emergency_stop) {
            ESP_LOGW(TAG, "legs_move_smooth 被紧急停止中断");
            return;
        }

        /* 取锁 → 写完这一小步所有舵机 → 立即释放锁 */
        if (s_servo_mutex != NULL) {
            xSemaphoreTake(s_servo_mutex, pdMS_TO_TICKS(200));
        }
        for (int i = 0; i < n; i++) {
            if (!legs[i]) continue;
            int32_t cur = starts[i] + (targets[i] - starts[i]) * s / steps;
            leg_set_angle(legs[i], (uint32_t)cur);
        }
        if (s_servo_mutex != NULL) {
            xSemaphoreGive(s_servo_mutex);
        }

        /* 等待到本步应结束的时间戳 */
        int64_t step_target_us = step0_us + (int64_t)s * step_ms * 1000LL;
        while (1) {
            int64_t now_us = esp_timer_get_time();
            if (now_us >= step_target_us) break;
            int64_t remain_us = step_target_us - now_us;
            /* 剩余 >5ms 时让出 CPU 一个 tick，让其他任务运行 */
            if (remain_us > 5000) {
                vTaskDelay(1);
            }
            /* remain_us ≤5ms：进入 busy-wait，保证微秒级精度 */
        }
    }

}

/* ================================================================
 * 四腿舵机实例（全局，静态初始化）
 * ================================================================ */
static servo_leg_t s_legs[4] = {
    [0] = { .ch = SERVO_CH_FR, .gpio = SERVO_GPIO_FR, .angle = SERVO_ANGLE_HOME, .lift = SERVO_LIFT_FIXED }, /* FR 右前 */
    [1] = { .ch = SERVO_CH_BR, .gpio = SERVO_GPIO_BR, .angle = SERVO_ANGLE_HOME, .lift = SERVO_LIFT_FIXED }, /* BR 右后 */
    [2] = { .ch = SERVO_CH_BL, .gpio = SERVO_GPIO_BL, .angle = SERVO_ANGLE_HOME, .lift = SERVO_LIFT_FIXED }, /* BL 左后 */
    [3] = { .ch = SERVO_CH_FL, .gpio = SERVO_GPIO_FL, .angle = SERVO_ANGLE_HOME, .lift = SERVO_LIFT_FIXED }, /* FL 左前 */
};

/* 宏：快速索引四腿 */
#define LEG_FR  (&s_legs[0])
#define LEG_BR  (&s_legs[1])
#define LEG_BL  (&s_legs[2])
#define LEG_FL  (&s_legs[3])

/* ================================================================
 * LEDC 初始化（全局定时器只初始化一次）
 * ================================================================ */
static void ledc_timer_init(void)
{
    static bool timer_ready = false;
    if (timer_ready) return;

    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_BIT_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    timer_ready = true;
}

static void ledc_channel_init(servo_leg_t *leg)
{
    ledc_channel_config_t c = {
        .gpio_num   = leg->gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = leg->ch,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = angle_to_duty_clamped(SERVO_ANGLE_HOME),
        .hpoint     = 0,
    };
    ledc_channel_config(&c);
}

/* ================================================================
 * 行走状态
 * ================================================================ */
static volatile walk_state_t s_walk_state = WALK_STATE_IDLE;
static TaskHandle_t s_walk_task_handle = NULL;

/* 异步动作任务句柄（由 websocket_control_server 设置，供紧急停止等待） */
static TaskHandle_t s_action_task_handle = NULL;

void servo_set_action_task_handle(void* handle) {
    s_action_task_handle = (TaskHandle_t)handle;
}

/* ================================================================
 * 6拍 Trot 对角前进步态
 *
 * 角度极性（来自 eda_dog_movements.cc）：
 *   左侧腿(FL/BL)：减小=向前，增大=向后
 *   右侧腿(FR/BR)：增大=向前，减小=向后
 *
 * 对角A：FL + BR  对角B：FR + BL
 *
 * 腿布局（俯视，机头朝上）：
 *   FL(47) ----前---- FR(9)
 *     |                  |
 *   BL(21) ----后---- BR(10)
 * ================================================================ */
void servo_one_step(void)
{
    int32_t lift  = (int32_t)WALK_LIFT_DEGREE;
    int32_t swing = (int32_t)WALK_STEP_SWING;
    int32_t t     = (int32_t)(WALK_STEP_PERIOD_MS / 6);
    int32_t home  = (int32_t)SERVO_ANGLE_HOME;

    servo_leg_t *legs[4];
    int32_t      tgt[4];

    /* ---- 拍1：抬 FL + BR（对角A离地）---- */
    legs[0] = LEG_FL; tgt[0] = home - lift;
    legs[1] = LEG_BR; tgt[1] = home - lift;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍2：FL/BR 空中前摆，FR/BL 地面后推 ---- */
    legs[0] = LEG_FL; tgt[0] = home - lift - swing;  /* FL 向前摆 */
    legs[1] = LEG_FR; tgt[1] = home - swing;          /* FR 向后推 */
    legs[2] = LEG_BL; tgt[2] = home + swing;          /* BL 向后推 */
    legs[3] = LEG_BR; tgt[3] = home - lift + swing;  /* BR 向前摆 */
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍3：放下 FL/BR 落地 ---- */
    legs[0] = LEG_FL; tgt[0] = home - swing;
    legs[1] = LEG_BR; tgt[1] = home + swing;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍4：抬 FR + BL（对角B离地）---- */
    legs[0] = LEG_FR; tgt[0] = home + lift;
    legs[1] = LEG_BL; tgt[1] = home + lift;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍5：FR/BL 空中前摆，FL/BR 地面后推 ---- */
    legs[0] = LEG_FL; tgt[0] = home + swing;           /* FL 向后推 */
    legs[1] = LEG_FR; tgt[1] = home + lift + swing;    /* FR 向前摆 */
    legs[2] = LEG_BL; tgt[2] = home + lift - swing;    /* BL 向前摆 */
    legs[3] = LEG_BR; tgt[3] = home - swing;           /* BR 向后推 */
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍6：放下 FR/BL 落地 ---- */
    legs[0] = LEG_FR; tgt[0] = home + swing;
    legs[1] = LEG_BL; tgt[1] = home - swing;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);
}

/* ================================================================
 * 6拍 Trot 对角倒退步态
 *
 * 与前进完全镜像：摆动方向取反
 *   空中腿向后摆，支撑腿向前推
 * ================================================================ */
void servo_one_step_back(void)
{
    int32_t lift  = (int32_t)WALK_LIFT_DEGREE;
    int32_t swing = (int32_t)WALK_STEP_SWING;
    int32_t t     = (int32_t)(WALK_STEP_PERIOD_MS / 6);
    int32_t home  = (int32_t)SERVO_ANGLE_HOME;

    servo_leg_t *legs[4];
    int32_t      tgt[4];

    /* ---- 拍1：抬 FL + BR（对角A离地）---- */
    legs[0] = LEG_FL; tgt[0] = home - lift;
    legs[1] = LEG_BR; tgt[1] = home - lift;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍2：FL/BR 空中后摆，FR/BL 地面前推（与前进相反）---- */
    legs[0] = LEG_FL; tgt[0] = home - lift + swing;  /* FL 向后摆 */
    legs[1] = LEG_FR; tgt[1] = home + swing;          /* FR 向前推 */
    legs[2] = LEG_BL; tgt[2] = home - swing;          /* BL 向前推 */
    legs[3] = LEG_BR; tgt[3] = home - lift - swing;  /* BR 向后摆 */
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍3：放下 FL/BR 落地 ---- */
    legs[0] = LEG_FL; tgt[0] = home + swing;
    legs[1] = LEG_BR; tgt[1] = home - swing;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍4：抬 FR + BL（对角B离地）---- */
    legs[0] = LEG_FR; tgt[0] = home + lift;
    legs[1] = LEG_BL; tgt[1] = home + lift;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍5：FR/BL 空中后摆，FL/BR 地面前推 ---- */
    legs[0] = LEG_FL; tgt[0] = home - swing;           /* FL 向前推 */
    legs[1] = LEG_FR; tgt[1] = home + lift - swing;     /* FR 向后摆 */
    legs[2] = LEG_BL; tgt[2] = home + lift + swing;     /* BL 向后摆 */
    legs[3] = LEG_BR; tgt[3] = home + swing;            /* BR 向前推 */
    legs_move_smooth(legs, tgt, 4, t);

    /* ---- 拍6：放下 FR/BL 落地 ---- */
    legs[0] = LEG_FR; tgt[0] = home - swing;
    legs[1] = LEG_BL; tgt[1] = home + swing;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);
}

/* ================================================================
 * 4拍 归位左转步态（对角交替）
 *
 * 拍1：左后向后lift + 右前向前lift（对角A抬起）
 * 拍2：左前向前lift + 右后向后lift（对角B抬起）
 * 拍3：左后、右前归位落地
 * 拍4：左前、右后归位落地
 *
 * 周期 = PERIOD_MS，起始/结束均在 90°
 * ================================================================ */
void servo_one_step_turn_left(void)
{
    int32_t lift  = (int32_t)WALK_LIFT_DEGREE;
    int32_t t     = (int32_t)(WALK_STEP_PERIOD_MS / 4);
    int32_t home  = (int32_t)SERVO_ANGLE_HOME;

    servo_leg_t *legs[4];
    int32_t      tgt[4];

    /* 拍1：左后向后lift，右前向前lift */
    legs[0] = LEG_BL; tgt[0] = home + lift;    /* BL 向后 (左后=增大) */
    legs[1] = LEG_FR; tgt[1] = home + lift;    /* FR 向前 (右前=增大) */
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍2：左前向前lift，右后向后lift */
    legs[0] = LEG_FL; tgt[0] = home - lift;    /* FL 向前 (左前=减小) */
    legs[1] = LEG_BR; tgt[1] = home - lift;    /* BR 向后 (右后=减小) */
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍3：左后、右前归位 */
    legs[0] = LEG_BL; tgt[0] = home;
    legs[1] = LEG_FR; tgt[1] = home;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍4：左前、右后归位 */
    legs[0] = LEG_FL; tgt[0] = home;
    legs[1] = LEG_BR; tgt[1] = home;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);
}

/* ================================================================
 * 4拍 归位右转步态（对角交替，左转镜像）
 *
 * 拍1：右后向后lift + 左前向前lift（对角A抬起）
 * 拍2：右前向前lift + 左后向后lift（对角B抬起）
 * 拍3：右后、左前归位落地
 * 拍4：右前、左后归位落地
 *
 * 周期 = PERIOD_MS，起始/结束均在 90°
 * ================================================================ */
void servo_one_step_turn_right(void)
{
    int32_t lift  = (int32_t)WALK_LIFT_DEGREE;
    int32_t t     = (int32_t)(WALK_STEP_PERIOD_MS / 4);
    int32_t home  = (int32_t)SERVO_ANGLE_HOME;

    servo_leg_t *legs[4];
    int32_t      tgt[4];

    /* 拍1：右后向后lift，左前向前lift */
    legs[0] = LEG_BR; tgt[0] = home - lift;    /* BR 向后 (右后=减小) */
    legs[1] = LEG_FL; tgt[1] = home - lift;    /* FL 向前 (左前=减小) */
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍2：右前向前lift，左后向后lift */
    legs[0] = LEG_FR; tgt[0] = home + lift;    /* FR 向前 (右前=增大) */
    legs[1] = LEG_BL; tgt[1] = home + lift;    /* BL 向后 (左后=增大) */
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍3：右后、左前归位 */
    legs[0] = LEG_BR; tgt[0] = home;
    legs[1] = LEG_FL; tgt[1] = home;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);

    /* 拍4：右前、左后归位 */
    legs[0] = LEG_FR; tgt[0] = home;
    legs[1] = LEG_BL; tgt[1] = home;
    legs[2] = NULL;
    legs[3] = NULL;
    legs_move_smooth(legs, tgt, 4, t);
}

/* 后台持续行走任务（处理所有运动状态） */
static void walk_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "walk task started, state=%d", (int)s_walk_state);
    while (s_walk_state != WALK_STATE_IDLE && !s_emergency_stop) {
        switch (s_walk_state) {
            case WALK_STATE_WALKING:    servo_one_step();            break;
            case WALK_STATE_BACKING:    servo_one_step_back();       break;
            case WALK_STATE_TURN_LEFT:  servo_one_step_turn_left();  break;
            case WALK_STATE_TURN_RIGHT: servo_one_step_turn_right(); break;
            default: break;
        }
    }
    /* 无论正常停止还是急停，退出时都归位。
     * walk_task 已退出循环，不会再和 servo_emergency_stop() 并发写舵机。 */
    if (s_emergency_stop) {
        ESP_LOGI(TAG, "walk_task 急停退出，逐条腿归位");
        servo_reset_all_sequential();
    } else {
        servo_reset_all_smooth();
    }
    s_walk_task_handle = NULL;
    ESP_LOGI(TAG, "walk task stopped, all legs reset");
    vTaskDelete(NULL);
}

/* ================================================================
 * 公共 API 实现
 * ================================================================ */

esp_err_t servo_init(void)
{
    ledc_timer_init();
    s_servo_mutex = xSemaphoreCreateMutex();
    if (s_servo_mutex == NULL) {
        ESP_LOGE(TAG, "舵机互斥锁创建失败！");
    }
    for (int i = 0; i < 4; i++) {
        ledc_channel_init(&s_legs[i]);
        /* 逐条腿归位，腿间延时降低上电电源冲击 */
        if (s_servo_mutex != NULL) {
            xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
        }
        leg_set_angle(&s_legs[i], SERVO_ANGLE_HOME);
        if (s_servo_mutex != NULL) {
            xSemaphoreGive(s_servo_mutex);
        }
        ESP_LOGI(TAG, "GPIO%02d CH%d init -> 90° lift=%u",
                 s_legs[i].gpio, s_legs[i].ch, s_legs[i].lift);
        /* 上电时腿间延时 80ms，避免四舵机同时大力拉动导致 IP5306 断电 */
        if (i < 3) vTaskDelay(pdMS_TO_TICKS(80));
    }
    ESP_LOGI(TAG, "四腿舵机初始化完成（GPIO 9/10/21/47）");
    return ESP_OK;
}

/* ---- 单腿角度 ---- */
void servo_set_angle_fr(uint32_t angle) { leg_set_angle(LEG_FR, angle); }
void servo_set_angle_br(uint32_t angle) { leg_set_angle(LEG_BR, angle); }
void servo_set_angle_bl(uint32_t angle) { leg_set_angle(LEG_BL, angle); }
void servo_set_angle_fl(uint32_t angle) { leg_set_angle(LEG_FL, angle); }

/* 抬腿偏移已固定为 SERVO_LIFT_FIXED=15°，不再提供运行时修改接口 */

/* ---- 查询 ---- */
uint32_t servo_get_angle_fr(void) { return LEG_FR->angle; }
uint32_t servo_get_angle_br(void) { return LEG_BR->angle; }
uint32_t servo_get_angle_bl(void) { return LEG_BL->angle; }
uint32_t servo_get_angle_fl(void) { return LEG_FL->angle; }

/* ---- 归位（瞬间，仅供初始化内部使用） ---- */
void servo_reset_all(void)
{
    if (s_servo_mutex != NULL) {
        xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < 4; i++) {
        leg_set_angle(&s_legs[i], SERVO_ANGLE_HOME);
    }
    if (s_servo_mutex != NULL) {
        xSemaphoreGive(s_servo_mutex);
    }
}

/* ---- 平滑归位（四腿同时，动作正常完成后调用，保持舞蹈节拍） ---- */
void servo_reset_all_smooth(void)
{
    servo_leg_t *legs[4] = { LEG_FR, LEG_BR, LEG_BL, LEG_FL };
    int32_t      tgt[4]  = {
        (int32_t)SERVO_ANGLE_HOME,
        (int32_t)SERVO_ANGLE_HOME,
        (int32_t)SERVO_ANGLE_HOME,
        (int32_t)SERVO_ANGLE_HOME
    };
    /* 归位时间使用一拍步长，保证过渡速度与步态节奏一致 */
    int32_t reset_ms = (int32_t)(WALK_STEP_PERIOD_MS / 6);
    legs_move_smooth(legs, tgt, 4, reset_ms);
}

/* ---- 逐条腿依次归位（上电/急停后调用，降低电源冲击） ---- */
void servo_reset_all_sequential(void)
{
    servo_leg_t *order[4] = { LEG_FR, LEG_BR, LEG_BL, LEG_FL };
    int32_t      home     = (int32_t)SERVO_ANGLE_HOME;
    int32_t      reset_ms = (int32_t)(WALK_STEP_PERIOD_MS / 6);

    ESP_LOGI(TAG, "逐条腿归位: FR→BR→BL→FL");
    for (int i = 0; i < 4; i++) {
        servo_leg_t *one[4] = { NULL, NULL, NULL, NULL };
        int32_t      tgt[4]  = { 0, 0, 0, 0 };
        one[0] = order[i];
        tgt[0] = home;
        legs_move_smooth(one, tgt, 4, reset_ms);
        /* 腿间延时，让电源恢复，降低 IP5306 过流保护风险 */
        if (i < 3) vTaskDelay(pdMS_TO_TICKS(80));
    }
}




/* ---- 遥控行走 ---- */

/* 内部：以指定状态启动 walk_task（若已在运行则仅切换状态） */
static void walk_start_with_state(walk_state_t state)
{
    /* 用互斥锁保护「检查句柄 + 创建任务」的原子性 */
    if (s_servo_mutex == NULL) {
        s_walk_state = state;
        xTaskCreatePinnedToCore(&walk_task, "walk_task", 4096, NULL, 5,
                                &s_walk_task_handle, 0);
        return;
    }

    if (xSemaphoreTake(s_servo_mutex, 0) == pdTRUE) {
        /* 临界区内重新检查 */
        if (s_walk_task_handle != NULL) {
            /* 任务已在运行，直接切换方向 */
            s_walk_state = state;
            xSemaphoreGive(s_servo_mutex);
            return;
        }
        s_walk_state = state;
        /* 持有锁期间创建任务，防止竞态 */
        BaseType_t ret = xTaskCreatePinnedToCore(&walk_task, "walk_task", 4096,
                                                NULL, 5, &s_walk_task_handle, 0);
        xSemaphoreGive(s_servo_mutex);

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "walk_task 创建失败！");
            s_walk_state = WALK_STATE_IDLE;
            s_walk_task_handle = NULL;
        }
    } else {
        /* 取锁失败说明另一任务正在操作，直接切换方向 */
        s_walk_state = state;
    }
}

void servo_walk_start(void)       { walk_start_with_state(WALK_STATE_WALKING); }
void servo_walk_back_start(void)  { walk_start_with_state(WALK_STATE_BACKING); }
void servo_turn_left_start(void)  { walk_start_with_state(WALK_STATE_TURN_LEFT); }
void servo_turn_right_start(void) { walk_start_with_state(WALK_STATE_TURN_RIGHT); }

void servo_walk_stop(void)
{
    if (s_walk_state == WALK_STATE_IDLE && s_walk_task_handle == NULL) return;
    s_walk_state = WALK_STATE_IDLE;
    /* 任务会在当前步结束后检测到 IDLE，自动停止并归位
     * 如果 s_emergency_stop 已置位，legs_move_smooth 会在步内提前退出 */
}

walk_state_t servo_walk_get_state(void)
{
    return s_walk_state;
}

/* ================================================================
 * 紧急停止：立即中断所有运动（行走+摇摆），让各执行路径自行归位
 *
 * 设计原则：
 *   - 只设标志，不抢锁，不强制杀任务（避免持锁被杀 → 死锁）
 *   - 不在此函数中操作舵机（避免与正在执行的 legs_move_smooth 并发写舵机）
 *   - walk_task / 摇摆函数 / legs_move_smooth 各自检测标志后自行退出并归位
 *
 * 工作流程：
 *   1. 置急停标志 → 所有执行路径在下一检查点感知
 *   2. 设行走状态为 IDLE → walk_task 在循环头部检测到 IDLE 退出
 *   3. 等待 walk_task 自然退出（最多 500ms）
 *   4. 等待异步动作任务退出（最多 2s）
 *   5. 若所有任务已退出，清除急停标志
 *
 * 注意：此函数不操作舵机，归位由最后退出的执行路径负责
 *       （walk_task / 摇摆函数 的 emergency_exit 会调 servo_reset_all_smooth）
 * ================================================================ */
void servo_emergency_stop(void)
{
    ESP_LOGW(TAG, "⚠ 紧急停止触发！");

    /* 1. 置急停标志（让 legs_move_smooth 和所有摇摆函数感知） */
    s_emergency_stop = true;

    /* 2. 设行走状态为 IDLE → walk_task 在循环头部检测到 IDLE 退出 */
    s_walk_state = WALK_STATE_IDLE;

    /* 3. 等待 walk_task 自然退出（不强制 vTaskDelete，避免持锁被杀） */
    int wait_count = 0;
    while (s_walk_task_handle != NULL && wait_count < 10) {
        vTaskDelay(pdMS_TO_TICKS(50));
        wait_count++;
    }
    if (s_walk_task_handle != NULL) {
        ESP_LOGW(TAG, "walk_task 500ms 内未退出（可能被阻塞）");
    } else {
        ESP_LOGI(TAG, "walk_task 已退出");
    }

    /* 4. 等待异步动作任务（dog_action）自然退出 */
    wait_count = 0;
    while (s_action_task_handle != NULL && wait_count < 40) {
        vTaskDelay(pdMS_TO_TICKS(50));
        wait_count++;
    }
    if (s_action_task_handle != NULL) {
        ESP_LOGW(TAG, "动作任务 2s 内未退出，舵机可能未归位");
    } else {
        ESP_LOGI(TAG, "动作任务已退出");
    }

    /* 5. 所有执行路径已退出，清除急停标志
     *    注意：如果 walk_task 或动作任务在 servo_emergency_stop 之后才检测到标志，
     *    它们会正常退出并归位（标志已被清除，但退出条件也检查 IDLE/循环结束） */
    s_emergency_stop = false;
    ESP_LOGI(TAG, "紧急停止完成");
}

/* ================================================================
 * 内部辅助：等待 walk_task 停止并退出（最多 wait_ms）
 * 检查任务句柄而非状态，确保任务已实际退出
 * ================================================================ */
static void wait_walk_idle(int wait_ms)
{
    int waited = 0;
    while (s_walk_task_handle != NULL && waited < wait_ms) {
        vTaskDelay(pdMS_TO_TICKS(50));
        waited += 50;
    }
    if (s_walk_task_handle != NULL) {
        ESP_LOGW(TAG, "wait_walk_idle 超时，任务可能仍在运行");
    }
}

/* 公开 API：等待行走任务实际退出 */
void servo_wait_walk_idle(int timeout_ms)
{
    wait_walk_idle(timeout_ms);
}

/* ================================================================
 * 前后摇摆
 *
 * 角度极性：
 *   右侧(FR/BR)：增大=向前；左侧(FL/BL)：减小=向前
 *   因此「机身前倾」= 重心前移 = 4 腿同时向后推
 *     右侧腿减小（向后），左侧腿增大（向后）
 *     即：FR/BR → 90 - delta，FL/BL → 90 + delta   → 机身前倾
 *         FR/BR → 90 + delta，FL/BL → 90 - delta   → 机身后倾
 * ================================================================ */
void servo_swing_fb(int amplitude, int cycles, int half_ms)
{
    /* 1. 停止当前行走任务 */
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        if (!s_emergency_stop) wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    /* 限幅：前后摇摆限定最大 90±15° */
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 15) amplitude = 15;

    servo_leg_t *all4[4] = { LEG_FR, LEG_BR, LEG_BL, LEG_FL };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_fb 被紧急停止中断"); goto emergency_exit; }

        /* 半拍1：机身前倾 → FR/BR 减小，FL/BL 增大 */
        int32_t tgt_fwd[4] = {
            home - amplitude,   /* FR 向后 → 机身前倾 */
            home - amplitude,   /* BR 向后 → 机身前倾 */
            home + amplitude,   /* BL 向后（左侧极性相反）→ 机身前倾 */
            home + amplitude    /* FL 向后 → 机身前倾 */
        };
        legs_move_smooth(all4, tgt_fwd, 4, half_ms);

        /* 半拍2：机身后倾 → FR/BR 增大，FL/BL 减小 */
        int32_t tgt_bwd[4] = {
            home + amplitude,
            home + amplitude,
            home - amplitude,
            home - amplitude
        };
        legs_move_smooth(all4, tgt_bwd, 4, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 左右摇摆
 *
 * 向右倾（dir=1）：
 *   静止侧 = 右侧(FR/BR) 保持 90°（不动）
 *   动侧   = 左侧，FL 向前(减小)，BL 向后(增大) → 左侧伸展撑高
 *   结果：左侧高，右侧矮，机身右倾（向动侧倾）
 *
 * 向左倾（dir=-1）：对称
 *   静止侧 = 左侧(FL/BL) 保持 90°
 *   动侧   = 右侧，FR 向后(减小)，BR 向前(增大) → 右侧伸展
 *
 * 注意角度极性：
 *   FL(左前)：减小=向前，BL(左后)：增大=向后（左侧前后岔开撑高）
 *   FR(右前)：减小=向后，BR(右后)：增大=向前（右侧前后岔开撑高）
 * ================================================================ */
void servo_swing_lr(int amplitude, int dir, int cycles, int half_ms)
{
    /* 1. 停止当前行走任务 */
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        if (!s_emergency_stop) wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 45) amplitude = 45;

    /* 预计算：左侧岔开目标（FL向前/BL向后）和右侧岔开目标（FR向后/BR向前） */
    /* 左侧极性：减小=向前，增大=向后
     * 右侧极性：增大=向前，减小=向后                                     */
    int32_t tgt_left[4]  = {
        home - amplitude,   /* FL 向前 */
        home + amplitude,   /* BL 向后 */
        home,               /* FR 回中  */
        home                /* BR 回中  */
    };
    int32_t tgt_right[4] = {
        home,               /* FL 回中  */
        home,               /* BL 回中  */
        home + amplitude,   /* FR 向前（右侧增大=向前）*/
        home - amplitude    /* BR 向后（右侧减小=向后）*/
    };
    /* 四条腿顺序：FL, BL, FR, BR */
    servo_leg_t *all4[4] = { LEG_FL, LEG_BL, LEG_FR, LEG_BR };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_lr 被紧急停止中断"); goto emergency_exit; }

        /* 半拍1：dir 侧岔开，另一侧回中（四腿同时移动） */
        if (dir > 0) {
            legs_move_smooth(all4, tgt_left, 4, half_ms);
        } else {
            legs_move_smooth(all4, tgt_right, 4, half_ms);
        }

        /* 半拍2：换侧（另一侧岔开，dir 侧回中） */
        if (dir > 0) {
            legs_move_smooth(all4, tgt_right, 4, half_ms);
        } else {
            legs_move_smooth(all4, tgt_left, 4, half_ms);
        }
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 摇杆实时姿态控制（直接设置角度，无平滑）
 *
 * fb > 0 = 机身前倾，fb < 0 = 后倾
 * lr > 0 = 机身右倾，lr < 0 = 左倾
 *
 * 叠加逻辑：
 *   FR = 90 - fb - lr_contrib_right
 *   BR = 90 - fb - lr_contrib_right
 *   FL = 90 + fb + lr_contrib_left
 *   BL = 90 + fb + lr_contrib_left
 *   lr > 0(右倾) → 左侧岔开，右侧 lr_contrib = 0；左侧 FL-lr，BL+lr
 *   lr < 0(左倾) → 右侧岔开，左侧 lr_contrib = 0；右侧 FR+lr(减小=向后)，BR-lr(增大=向前)
 * ================================================================ */
void servo_body_sway(int fb, int lr)
{
    /* 先停止行走并等待任务实际退出 */
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        if (!s_emergency_stop) wait_walk_idle(3000);
    }

    /* 取互斥锁，独占舵机 */
    if (s_servo_mutex != NULL) {
        xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;

    /* 前后分量：机身前倾(fb>0) → FR/BR 减小，FL/BL 增大 */
    int32_t fr_angle = home - fb;
    int32_t br_angle = home - fb;
    int32_t fl_angle = home + fb;
    int32_t bl_angle = home + fb;

    /* 左右分量叠加 */
    if (lr > 0) {
        /* 右倾：左侧(FL/BL)前后岔开，右侧不加分量 */
        fl_angle -= lr;   /* FL 向前 */
        bl_angle += lr;   /* BL 向后 */
    } else if (lr < 0) {
        /* 左倾：右侧(FR/BR)前后岔开，左侧不加分量 */
        int abs_lr = -lr;
        fr_angle -= abs_lr;   /* FR 向后（减小） */
        br_angle += abs_lr;   /* BR 向前（增大） */
    }

    leg_set_angle(LEG_FR, (uint32_t)fr_angle);
    leg_set_angle(LEG_BR, (uint32_t)br_angle);
    leg_set_angle(LEG_BL, (uint32_t)bl_angle);
    leg_set_angle(LEG_FL, (uint32_t)fl_angle);

    /* 释放互斥锁 */
    if (s_servo_mutex != NULL) {
        xSemaphoreGive(s_servo_mutex);
    }
}

/* ================================================================
 * 旋转摇摆（身体扭转）
 *
 * 扭转原理：
 *   右侧腿(FR,BR)全部向前，左侧腿(FL,BL)全部向后 → 身体扭转
 *   下一半拍反相（右侧向后，左侧向前）→ 身体反向扭转
 *
 * 角度极性：
 *   右侧(FR/BR)：增大=向前，减小=向后
 *   左侧(FL/BL)：减小=向前，增大=向后
 *
 * 扭转状态1（右侧向前、左侧向后）：
 *   FR = home + amplitude  （右前腿 向前）
 *   BR = home + amplitude  （右后腿 向前）
 *   BL = home + amplitude  （左后腿 向后：左侧增大=向后）
 *   FL = home + amplitude  （左前腿 向后：左侧增大=向后）
 * 扭转状态2（反相：右侧向后，左侧向前）：
 *   全部取反，四腿统一用 home - amplitude
 * ================================================================ */
void servo_swing_twist(int amplitude, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 25) amplitude = 25;

    /* 四腿顺序：FR, BR, BL, FL */
    servo_leg_t *all4[4] = { LEG_FR, LEG_BR, LEG_BL, LEG_FL };

    /* 扭转状态1：右侧(FR/BR)向前，左侧(FL/BL)向后 */
    int32_t tgt_twist1[4] = {
        home + amplitude,   /* FR 向前（右侧增大=向前） */
        home + amplitude,   /* BR 向前（右侧增大=向前） */
        home + amplitude,   /* BL 向后（左侧增大=向后） */
        home + amplitude    /* FL 向后（左侧增大=向后） */
    };
    /* 扭转状态2：反相 — 右侧向后，左侧向前 */
    int32_t tgt_twist2[4] = {
        home - amplitude,   /* FR 向后（右侧减小=向后） */
        home - amplitude,   /* BR 向后（右侧减小=向后） */
        home - amplitude,   /* BL 向前（左侧减小=向前） */
        home - amplitude    /* FL 向前（左侧减小=向前） */
    };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_twist 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(all4, tgt_twist1, 4, half_ms);
        legs_move_smooth(all4, tgt_twist2, 4, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 上下摇摆（蹲起）
 *
 * 原理：四腿同时前后岔开 → 机身下沉（蹲）
 *       再全部回到 home 90° → 机身抬起（起）
 *
 * 岔开状态：
 *   右侧(FR)：向前 = home + amplitude
 *   右侧(BR)：向后 = home - amplitude
 *   左侧(FL)：向前 = home - amplitude（极性相反）
 *   左侧(BL)：向后 = home + amplitude（极性相反）
 * ================================================================ */
void servo_swing_updown(int amplitude, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 20) amplitude = 20;

    servo_leg_t *all4[4] = { LEG_FR, LEG_BR, LEG_BL, LEG_FL };

    /* 岔开目标（下沉）：前后腿各向相反方向撑开 */
    int32_t tgt_open[4] = {
        home + amplitude,   /* FR 向前 */
        home - amplitude,   /* BR 向后 */
        home + amplitude,   /* BL 向后（左侧增大=向后） */
        home - amplitude    /* FL 向前（左侧减小=向前） */
    };
    /* 归中目标（抬起）*/
    int32_t tgt_home[4] = { home, home, home, home };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_updown 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(all4, tgt_open, 4, half_ms);   /* 蹲 */
        legs_move_smooth(all4, tgt_home, 4, half_ms);   /* 起 */
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 左侧侧摇：左腿岔开↔反岔开交替，右腿保持岔开不动
 *
 * 右腿（不动侧）保持向内收固定岔开：
 *   FR(右前)  减小=向后  → FR = home - in_deg
 *   BR(右后)  增大=向前  → BR = home + in_deg
 *
 * 左腿（摇摆侧）在两种状态之间交替（同极性，同时向前或同时向后）：
 *   状态A: FL+BL 同时向前  →  FL = home - amplitude, BL = home - amplitude
 *   状态B: FL+BL 同时向后  →  FL = home + amplitude, BL = home + amplitude
 * ================================================================ */
void servo_swing_side_left(int amplitude, int in_deg, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 40) amplitude = 40;
    if (in_deg < 0)     in_deg = 0;
    if (in_deg > 40)    in_deg = 40;

    servo_leg_t *all4[4] = { LEG_FL, LEG_BL, LEG_FR, LEG_BR };

    int32_t tgt_a[4] = {
        home - amplitude,  /* FL 向前（摇摆侧，同极性） */
        home - amplitude,  /* BL 向前（摇摆侧，同极性） */
        home - in_deg,     /* FR 向后（不动侧，固定） */
        home + in_deg      /* BR 向前（不动侧，固定） */
    };
    int32_t tgt_b[4] = {
        home + amplitude,  /* FL 向后（摇摆侧，同极性反相）*/
        home + amplitude,  /* BL 向后（摇摆侧，同极性反相）*/
        home - in_deg,     /* FR 向后（不动侧，固定） */
        home + in_deg      /* BR 向前（不动侧，固定） */
    };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_side_left 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(all4, tgt_a, 4, half_ms);
        legs_move_smooth(all4, tgt_b, 4, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 右侧侧摇：右腿岔开↔反岔开交替，左腿保持岔开不动
 *
 * 左腿（不动侧）保持向内收固定岔开：
 *   FL(左前)  增大=向后  → FL = home + in_deg
 *   BL(左后)  减小=向前  → BL = home - in_deg
 *
 * 右腿（摇摆侧）在两种状态之间交替（同极性，同时向前或同时向后）：
 *   状态A: FR+BR 同时向前  →  FR = home + amplitude, BR = home + amplitude
 *   状态B: FR+BR 同时向后  →  FR = home - amplitude, BR = home - amplitude
 * ================================================================ */
void servo_swing_side_right(int amplitude, int in_deg, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 40) amplitude = 40;
    if (in_deg < 0)     in_deg = 0;
    if (in_deg > 40)    in_deg = 40;

    servo_leg_t *all4[4] = { LEG_FL, LEG_BL, LEG_FR, LEG_BR };

    int32_t tgt_a[4] = {
        home + in_deg,     /* FL 向后（不动侧，固定） */
        home - in_deg,     /* BL 向前（不动侧，固定） */
        home + amplitude,  /* FR 向前（摇摆侧，同极性） */
        home + amplitude   /* BR 向前（摇摆侧，同极性） */
    };
    int32_t tgt_b[4] = {
        home + in_deg,     /* FL 向后（不动侧，固定） */
        home - in_deg,     /* BL 向前（不动侧，固定） */
        home - amplitude,  /* FR 向后（摇摆侧，同极性反相） */
        home - amplitude   /* BR 向后（摇摆侧，同极性反相） */
    };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_side_right 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(all4, tgt_a, 4, half_ms);
        legs_move_smooth(all4, tgt_b, 4, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}




/* ================================================================
 * 波浪步：两圈接力
 *
 * 圈1（FL→BL→BR→FR）：左前腿先伸，接力到左后→右后→右前
 * 圈2（BR→BL→FL）    ：右后腿先伸，接力到左后→左前结束
 *
 * 圈1 五段（4腿 + 归中），圈2 四段（3腿 + 归中），四腿一起动。
 * ================================================================ */
void servo_swing_wave(int amplitude, int cycles, int step_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 30) amplitude = 30;

    servo_leg_t *all4[4] = { LEG_FL, LEG_FR, LEG_BL, LEG_BR };

    /* ── 圈1：FL→BL→BR→FR ──
     * all4 顺序：FL(0), FR(1), BL(2), BR(3) */
    int32_t c1_fl[4]    = { home - amplitude,  home,             home,             home             };
    int32_t c1_bl[4]    = { home,             home,             home - amplitude,  home             };
    int32_t c1_br[4]    = { home,             home,             home,             home + amplitude  };
    int32_t c1_fr[4]    = { home,             home + amplitude,  home,             home             };
    int32_t home_all[4] = { home,             home,             home,             home             };

    /* ── 圈2：BR→BL→FL ──
     * all4 顺序：FL(0), FR(1), BL(2), BR(3) */
    int32_t c2_br[4]    = { home,             home,             home,             home + amplitude  };
    int32_t c2_bl[4]    = { home,             home,             home - amplitude,  home             };
    int32_t c2_fl[4]    = { home - amplitude,  home,             home,             home             };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_wave 被紧急停止中断"); goto emergency_exit; }

        /* 圈1：FL → BL → BR → FR */
        legs_move_smooth(all4, c1_fl,   4, step_ms);
        legs_move_smooth(all4, c1_bl,   4, step_ms);
        legs_move_smooth(all4, c1_br,   4, step_ms);
        legs_move_smooth(all4, c1_fr,   4, step_ms);
        legs_move_smooth(all4, home_all, 4, step_ms);

        /* 圈2：BR → BL → FL */
        legs_move_smooth(all4, c2_br,   4, step_ms);
        legs_move_smooth(all4, c2_bl,   4, step_ms);
        legs_move_smooth(all4, c2_fl,   4, step_ms);
        legs_move_smooth(all4, home_all, 4, step_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 原地踏步：对角腿交替岔开，比行走步态更快更紧凑
 *
 * 拍1: FR+BL 岔开（前后分别向前/向后）→ 拍2: 归位
 * 拍3: FL+BR 岔开 → 拍4: 归位
 * ================================================================ */
void servo_swing_march(int amplitude, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 25) amplitude = 25;

    servo_leg_t *diag_a[2] = { LEG_FR, LEG_BL };
    int32_t diag_a_open[2] = {
        home + amplitude,  /* FR 向前（右侧增大=向前） */
        home + amplitude   /* BL 向后（左侧增大=向后） */
    };
    int32_t diag_a_home[2] = { home, home };

    servo_leg_t *diag_b[2] = { LEG_FL, LEG_BR };
    int32_t diag_b_open[2] = {
        home - amplitude,  /* FL 向前（左侧减小=向前） */
        home - amplitude   /* BR 向后（右侧减小=向后） */
    };
    int32_t diag_b_home[2] = { home, home };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_march 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(diag_a, diag_a_open, 2, half_ms);
        legs_move_smooth(diag_a, diag_a_home, 2, half_ms);
        legs_move_smooth(diag_b, diag_b_open, 2, half_ms);
        legs_move_smooth(diag_b, diag_b_home, 2, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 侧向点头：前腿左右交替前伸，后腿保持 90°
 *
 * 拍1: FL 前伸 + FR 后缩（头部偏左效果） → 拍2: 归位
 * 拍3: FR 前伸 + FL 后缩（头部偏右效果） → 拍4: 归位
 * ================================================================ */
void servo_swing_nod(int amplitude, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 25) amplitude = 25;

    servo_leg_t *front[2] = { LEG_FL, LEG_FR };
    /* FL-amp: FL 向前；FR-amp: FR 向后（右侧减小=向后） */
    int32_t tgt_left[2]  = {
        home - amplitude,  /* FL 向前 */
        home - amplitude   /* FR 向后 */
    };
    /* FL+amp: FL 向后；FR+amp: FR 向前 */
    int32_t tgt_right[2] = {
        home + amplitude,  /* FL 向后 */
        home + amplitude   /* FR 向前 */
    };
    int32_t tgt_home[2] = { home, home };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_nod 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(front, tgt_left,  2, half_ms);
        legs_move_smooth(front, tgt_home,  2, half_ms);
        legs_move_smooth(front, tgt_right, 2, half_ms);
        legs_move_smooth(front, tgt_home,  2, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}

/* ================================================================
 * 颤抖：四腿高频微幅对角交替，模拟颤抖/兴奋感
 *
 * 拍1: FR+BL 各 +amp（FR向前，BL向后）；FL+BR 各 -amp（FL向后，BR向后）
 * 拍2: 全部反相
 * ================================================================ */
void servo_swing_tremble(int amplitude, int cycles, int half_ms)
{
    if (s_walk_state != WALK_STATE_IDLE) {
        s_walk_state = WALK_STATE_IDLE;
        wait_walk_idle(3000);
    }

    int32_t home = (int32_t)SERVO_ANGLE_HOME;
    if (amplitude < 1)  amplitude = 1;
    if (amplitude > 15) amplitude = 15;

    servo_leg_t *all4[4] = { LEG_FR, LEG_BL, LEG_FL, LEG_BR };

    int32_t tgt_a[4] = {
        home + amplitude,  /* FR 向前（增大=向前） */
        home + amplitude,  /* BL 向后（增大=向后） */
        home - amplitude,  /* FL 向后（减小=向前 → +amp 取反 -amp = 向后） */
        home - amplitude   /* BR 向后（减小=向后） */
    };
    int32_t tgt_b[4] = {
        home - amplitude,
        home - amplitude,
        home + amplitude,
        home + amplitude
    };

    for (int c = 0; c < cycles; c++) {
        if (s_emergency_stop) { ESP_LOGW(TAG, "swing_tremble 被紧急停止中断"); goto emergency_exit; }

        legs_move_smooth(all4, tgt_a, 4, half_ms);
        legs_move_smooth(all4, tgt_b, 4, half_ms);
    }

    return;

    /* 急停退出：逐条腿依次归位（降低电源冲击） */
emergency_exit:
    servo_reset_all_sequential();
}
