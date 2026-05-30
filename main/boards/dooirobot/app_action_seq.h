#ifndef APP_ACTION_SEQ_H_
#define APP_ACTION_SEQ_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 动作类型 ==================== */
typedef enum {
    ACTION_NONE = 0,
    ACTION_EXPRESSION,
    ACTION_EYE_MOVE,
    ACTION_EYE_WINK,
    ACTION_LED_SET,
    ACTION_LED_BREATHE,
    ACTION_LED_BLINK,
    ACTION_LED_RAINBOW,
    ACTION_LED_OFF,
    ACTION_SERVO_MOVE,
    ACTION_MOTOR_CONTROL,
    ACTION_DELAY,
    ACTION_TYPE_MAX
} action_type_t;

/* ==================== 动作命令 ==================== */
typedef struct {
    action_type_t type;
    union {
        struct { uint16_t emotion_id; }                                expression;
        struct { float x, y; uint16_t duration; }                      eye_move;
        struct { uint32_t left_color, right_color; }                   led_set;
        struct { uint32_t color; uint16_t period; bool symmetric; }    led_breathe;
        struct { uint32_t color; uint16_t period_ms; bool symmetric; } led_blink;
        struct { uint16_t period_ms; bool symmetric; }                 led_rainbow;
        struct { uint8_t servo_id; uint8_t angle; uint16_t duration_ms; } servo_move;
        struct { int16_t left_speed, right_speed; uint16_t duration; } motor_control;
        struct { uint16_t duration; }                                  delay;
    } params;
} action_cmd_t;

/* ==================== 优先级 ==================== */
typedef enum {
    PRIO_IDLE = 0,
    PRIO_LOW,
    PRIO_NORMAL,
    PRIO_HIGH,
    PRIO_CRITICAL
} action_priority_t;

/* ==================== 复位策略 ==================== */
typedef enum {
    RESET_NONE    = 0,   /* 执行完不复位（默认）          */
    RESET_AFTER   = 1,   /* 执行完延迟后复位到 reset_angle */
} servo_reset_policy_t;

/* ==================== 序列定义 ==================== */
typedef struct {
    const action_cmd_t   *steps;
    uint16_t              count;
    action_priority_t     priority;
    bool                  interruptible;

    /* ---- 复位配置 ---- */
    servo_reset_policy_t  reset_policy;   /* 是否在序列结束后自动复位 */
    uint16_t              reset_delay_ms; /* 复位前等待时长，默认 1000 ms */
    uint8_t               reset_angle;   /* 复位目标角度，默认 90°      */
} action_seq_def_t;

/* ==================== 序列 ID ==================== */
typedef enum {
    /* ---- 情绪类 ---- */
    SEQ_SURPRISED,
    SEQ_ANGRY,
    SEQ_ANGRY_ALT,
    SEQ_ANGRY_FOCUS_LEFT,
    SEQ_ANGRY_FOCUS_RIGHT,
    SEQ_HAPPY,
    SEQ_HAPPY_ALT,
    SEQ_HAPPY_ALT2,
    SEQ_SAD,
    SEQ_DISDAIN,
    SEQ_FEAR,
    SEQ_THINKING,
    SEQ_CONFUSED,
    SEQ_SHY,
    SEQ_BORED,
    SEQ_LOVE,
    SEQ_CURIOUS,

    /* ---- 注视类 ---- */
    SEQ_FOCUS_BELOW_CENTER,
    SEQ_FOCUS_TOP_CENTER,
    SEQ_FOCUS_TOP_LEFT,
    SEQ_FOCUS_TOP_RIGHT,
    SEQ_FOCUS_LEFT,
    SEQ_FOCUS_RIGHT,

    /* ---- 肢体动作类 ---- */
    SEQ_DANCE,
    SEQ_GREETING,
    SEQ_WAVE_GOODBYE,
    SEQ_HUG,
    SEQ_NOD_AGREE,
    SEQ_SHAKE_NO,
    SEQ_STRETCH,
    SEQ_WELCOME,
    SEQ_MORNING,
    SEQ_GOODNIGHT,
    SEQ_ENCOURAGE,

    /* ---- 页面默认动作 ---- */
    SEQ_PAGE_MUSIC,
    SEQ_PAGE_LUCKY_CAT,
    SEQ_PAGE_CAMERA,
    SEQ_PAGE_CLOCK,
    SEQ_PAGE_INFO,
    SEQ_PAGE_SELECT,
    SEQ_PAGE_GENERAL,

    SEQ_DOOI_RESET,

    SEQ_COUNT
} sequence_id_t;

/* ==================== 回调 ==================== */
typedef void (*action_done_cb_t)(sequence_id_t seq_id, bool was_interrupted);

/* ==================== 便捷宏 ==================== */

/* 表情 */
#define ACT_EXPR(emo) \
    {ACTION_EXPRESSION, {.expression = {.emotion_id = (emo)}}}

/* 眼球移动 */
#define ACT_EYE(px, py, dur) \
    {ACTION_EYE_MOVE, {.eye_move = {.x = (px), .y = (py), .duration = (dur)}}}

/* 眨眼 */
#define ACT_WINK() \
    {ACTION_EYE_WINK, {}}

/* LED 直接设色 */
#define ACT_LED(lc, rc) \
    {ACTION_LED_SET, {.led_set = {.left_color = (lc), .right_color = (rc)}}}

/* LED 呼吸 */
#define ACT_BREATHE(c, p, s) \
    {ACTION_LED_BREATHE, {.led_breathe = {.color = (c), .period = (p), .symmetric = (s)}}}

/* LED 闪烁 */
#define ACT_BLINK(c, p, s) \
    {ACTION_LED_BLINK, {.led_blink = {.color = (c), .period_ms = (p), .symmetric = (s)}}}

/* LED 彩虹 */
#define ACT_RAINBOW(p, s) \
    {ACTION_LED_RAINBOW, {.led_rainbow = {.period_ms = (p), .symmetric = (s)}}}

/* LED 关闭 */
#define ACT_LED_STOP() \
    {ACTION_LED_OFF, {}}

/* 舵机（含运动时长） */
#define ACT_SERVO(id, ang, dur) \
    {ACTION_SERVO_MOVE, {.servo_move = {.servo_id = (id), .angle = (ang), .duration_ms = (dur)}}}

/* 电机 */
#define ACT_MOTOR(ls, rs, dur) \
    {ACTION_MOTOR_CONTROL, {.motor_control = {.left_speed = (ls), .right_speed = (rs), .duration = (dur)}}}

/* 延时 */
#define ACT_WAIT(ms) \
    {ACTION_DELAY, {.delay = {.duration = (ms)}}}

/*
 * 序列定义宏
 *
 * DEFINE_SEQUENCE(name, prio, interr, ...)
 *   不带复位，序列执行完毕后舵机保持最后位置（与旧版兼容）。
 *
 * DEFINE_SEQUENCE_RESET(name, prio, interr, reset_delay_ms, reset_angle, ...)
 *   序列执行完毕后，等待 reset_delay_ms 毫秒，再把双臂复位到 reset_angle。
 *   常用：reset_delay_ms=1000, reset_angle=90
 */
#define DEFINE_SEQUENCE(name, prio, interr, ...) \
    static const action_cmd_t _steps_##name[] = { __VA_ARGS__ }; \
    static const action_seq_def_t _def_##name = { \
        .steps        = _steps_##name, \
        .count        = sizeof(_steps_##name) / sizeof(action_cmd_t), \
        .priority     = (prio), \
        .interruptible = (interr), \
        .reset_policy  = RESET_NONE, \
        .reset_delay_ms = 0, \
        .reset_angle   = 90, \
    }

#define DEFINE_SEQUENCE_RESET(name, prio, interr, rdel, rang, ...) \
    static const action_cmd_t _steps_##name[] = { __VA_ARGS__ }; \
    static const action_seq_def_t _def_##name = { \
        .steps        = _steps_##name, \
        .count        = sizeof(_steps_##name) / sizeof(action_cmd_t), \
        .priority     = (prio), \
        .interruptible = (interr), \
        .reset_policy  = RESET_AFTER, \
        .reset_delay_ms = (rdel), \
        .reset_angle   = (rang), \
    }

/* ==================== API ==================== */

void app_action_engine_init(void);

bool app_action_engine_play(sequence_id_t seq_id, action_done_cb_t on_done);

bool app_action_engine_play_custom(const action_cmd_t *steps, uint16_t count,
                               action_priority_t prio, action_done_cb_t on_done);

bool app_action_engine_stop(void);

bool app_action_engine_is_idle(void);

bool app_action_engine_play_from_isr(sequence_id_t seq_id);

const action_cmd_t* app_action_engine_prepare_custom(const action_cmd_t *steps, uint16_t count);

#ifdef __cplusplus
}
#endif

#endif /* APP_ACTION_SEQ_H_ */