#include "app_action_seq.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

#include "app_periphral.h"
#define TAG "act_engine"

#define ENGINE_TASK_STACK   4096
#define ENGINE_TASK_PRIO    5
#define ENGINE_QUEUE_DEPTH  8

#define CUSTOM_STEPS_MAX 64
static action_cmd_t *g_custom_steps = NULL;

/* ==================== 命令消息 ==================== */
typedef enum {
    CMD_PLAY,
    CMD_PLAY_CUSTOM,
    CMD_STOP,
} engine_cmd_type_t;

typedef struct {
    engine_cmd_type_t cmd;
    union {
        struct {
            sequence_id_t    seq_id;
            action_done_cb_t on_done;
        } play;
        struct {
            const action_cmd_t *steps;
            uint16_t            count;
            action_priority_t   prio;
            action_done_cb_t    on_done;
        } play_custom;
    };
} engine_msg_t;

/* ==================== Handler 类型 ==================== */
typedef void (*action_handler_fn)(const action_cmd_t *cmd);

/* ==================== Handler 实现 ==================== */

static void h_expression(const action_cmd_t *cmd) {
    ;
}

static void h_eye_move(const action_cmd_t *cmd) {
    ;
}

static void h_eye_wink(const action_cmd_t *cmd) {
    ;
}

static void h_led_set(const action_cmd_t *cmd) {
    app_led_set_rgb(cmd->params.led_set.left_color,
                    cmd->params.led_set.right_color);
}

static void h_led_breathe(const action_cmd_t *cmd) {
    app_led_set_breathing(cmd->params.led_breathe.color,
                          cmd->params.led_breathe.period,
                          cmd->params.led_breathe.symmetric);
}

static void h_led_blink(const action_cmd_t *cmd) {
    app_led_set_blink(cmd->params.led_blink.color,
                      cmd->params.led_blink.period_ms,
                      cmd->params.led_blink.symmetric);
}

static void h_led_rainbow(const action_cmd_t *cmd) {
    app_led_set_rainbow(cmd->params.led_rainbow.period_ms,
                        cmd->params.led_rainbow.symmetric);
}

static void h_led_off(const action_cmd_t *cmd) {
    app_led_off();
}

static void h_servo(const action_cmd_t *cmd) {
    app_servo_set_target_angle(cmd->params.servo_move.servo_id,
                               cmd->params.servo_move.angle,
                               cmd->params.servo_move.duration_ms);
}

static void h_motor(const action_cmd_t *cmd) {
    app_motor_move_auto(cmd->params.motor_control.left_speed,
                        cmd->params.motor_control.right_speed,
                        cmd->params.motor_control.duration);
}

/* ==================== Handler 注册表 ==================== */
static const action_handler_fn g_handlers[ACTION_TYPE_MAX] = {
    [ACTION_NONE]          = NULL,
    [ACTION_EXPRESSION]    = h_expression,
    [ACTION_EYE_MOVE]      = h_eye_move,
    [ACTION_EYE_WINK]      = h_eye_wink,
    [ACTION_LED_SET]       = h_led_set,
    [ACTION_LED_BREATHE]   = h_led_breathe,
    [ACTION_LED_BLINK]     = h_led_blink,
    [ACTION_LED_RAINBOW]   = h_led_rainbow,
    [ACTION_LED_OFF]       = h_led_off,
    [ACTION_SERVO_MOVE]    = h_servo,
    [ACTION_MOTOR_CONTROL] = h_motor,
    [ACTION_DELAY]         = NULL,      /* 引擎内部处理 */
};

/* ==================== 引擎状态 ==================== */

/*
 * 复位阶段状态机：
 *
 *   RESET_PHASE_NONE
 *     序列步骤全部执行完毕后：
 *       - 若 reset_policy == RESET_NONE  → 直接调用 finish_sequence()
 *       - 若 reset_policy == RESET_AFTER → 进入 RESET_PHASE_WAITING
 *
 *   RESET_PHASE_WAITING
 *     引擎进入延时等待（delay_start / delay_dur），
 *     等待 reset_delay_ms 结束后进入 RESET_PHASE_MOVING
 *
 *   RESET_PHASE_MOVING
 *     发出舵机复位指令（双臂 → reset_angle，duration=600ms），
 *     再等待 600ms（舵机运动时间），结束后调用 finish_sequence()
 */
typedef enum {
    RESET_PHASE_NONE    = 0,
    RESET_PHASE_WAITING,   /* 正在等待复位延时 */
    RESET_PHASE_MOVING,    /* 舵机正在复位运动 */
} reset_phase_t;

/* 舵机从任意位置归中的运动时长（ms） */
#define SERVO_RESET_MOVE_MS  600

static struct {
    QueueHandle_t   queue;
    TaskHandle_t    task;

    /* 当前序列 */
    const action_seq_def_t *seq;        /* NULL = 空闲 */
    sequence_id_t           seq_id;
    uint16_t                step_idx;
    action_priority_t       cur_prio;
    action_done_cb_t        on_done;

    /* 延时状态（序列内 ACT_WAIT 与复位阶段复用此字段） */
    bool                    in_delay;
    uint32_t                delay_start;
    uint16_t                delay_dur;

    /* 复位阶段 */
    reset_phase_t           reset_phase;

    /* 外部可查询 */
    volatile bool           idle;
} eng;

/* ==================== 工具函数 ==================== */

static inline uint32_t tick_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static TickType_t calc_timeout(void) {
    if (!eng.seq) {
        return portMAX_DELAY;
    }

    if (eng.in_delay) {
        uint32_t elapsed = tick_ms() - eng.delay_start;
        if (elapsed >= eng.delay_dur) {
            return 0;
        }
        return pdMS_TO_TICKS(eng.delay_dur - elapsed);
    }

    return 0;
}

/* ==================== 延时辅助 ==================== */

static void start_delay(uint16_t ms) {
    eng.in_delay    = true;
    eng.delay_start = tick_ms();
    eng.delay_dur   = ms;
}

/* ==================== 序列生命周期 ==================== */

static void finish_sequence(bool interrupted) {
    sequence_id_t    id = eng.seq_id;
    action_done_cb_t cb = eng.on_done;

    eng.seq         = NULL;
    eng.step_idx    = 0;
    eng.in_delay    = false;
    eng.cur_prio    = PRIO_IDLE;
    eng.on_done     = NULL;
    eng.idle        = true;
    eng.reset_phase = RESET_PHASE_NONE;

    // ESP_LOGI(TAG, "Seq %d finished%s", id, interrupted ? " (interrupted)" : "");

    if (cb) {
        cb(id, interrupted);
    }
}

static void stop_all_hw(void) {
    // app_motor_move(0, 0, 0);
    // app_led_off();
}

/* ==================== 复位阶段推进 ==================== */

/*
 * do_reset_tick() 在序列步骤全部跑完后、延时超时后被调用，
 * 每次调用推进复位状态机一步。
 */
static void do_reset_tick(void) {
    switch (eng.reset_phase) {

        case RESET_PHASE_NONE:
            /* 序列刚执行完，检查是否需要复位 */
            if (eng.seq->reset_policy == RESET_AFTER) {
                // ESP_LOGI(TAG, "Seq done, reset after %d ms -> angle %d",
                //          eng.seq->reset_delay_ms, eng.seq->reset_angle);
                eng.reset_phase = RESET_PHASE_WAITING;
                start_delay(eng.seq->reset_delay_ms);
            } else {
                /* 不复位，直接结束 */
                finish_sequence(false);
            }
            break;

        case RESET_PHASE_WAITING:
            /* 等待延时结束，发出舵机复位指令 */
            ESP_LOGI(TAG, "Reset delay done, moving servos to %d deg", eng.seq->reset_angle);
            app_servo_set_target_angle(SERVO_TARGET_BOTH,
                                       eng.seq->reset_angle,
                                       SERVO_RESET_MOVE_MS);
            eng.reset_phase = RESET_PHASE_MOVING;
            start_delay(SERVO_RESET_MOVE_MS);
            break;

        case RESET_PHASE_MOVING:
            /* 舵机运动完成，整个序列真正结束 */
            finish_sequence(false);
            break;
    }
}

/* ==================== 步骤推进 ==================== */

static void advance(void) {
    /* 还有步骤未执行 */
    while (eng.seq && eng.step_idx < eng.seq->count) {
        const action_cmd_t *step = &eng.seq->steps[eng.step_idx];

        if (step->type == ACTION_DELAY) {
            start_delay(step->params.delay.duration);
            eng.step_idx++;
            ESP_LOGD(TAG, "Delay %d ms", eng.delay_dur);
            return;
        }

        if (step->type < ACTION_TYPE_MAX && g_handlers[step->type]) {
            g_handlers[step->type](step);
        } else {
            ESP_LOGW(TAG, "Unknown action type: %d", step->type);
        }

        eng.step_idx++;
    }

    /* 所有步骤跑完，进入复位阶段 */
    if (eng.seq && eng.step_idx >= eng.seq->count) {
        do_reset_tick();   /* 第一次调用：决定进 WAITING 还是直接 finish */
    }
}

/* ==================== 引擎主循环超时回调 ==================== */

/*
 * 每次 xQueueReceive 超时（即 in_delay 到期）都会走到这里。
 */
static void on_delay_expired(void) {
    eng.in_delay = false;

    if (!eng.seq) return;

    /* 判断当前处于序列内延时还是复位阶段延时 */
    if (eng.reset_phase != RESET_PHASE_NONE) {
        /* 复位阶段的延时超时 */
        do_reset_tick();
    } else {
        /* 序列内 ACT_WAIT 超时，继续推进步骤 */
        advance();
    }
}

/* ==================== 命令处理 ==================== */

static void start_sequence(const action_seq_def_t *def, sequence_id_t id,
                           action_done_cb_t on_done) {
    eng.seq         = def;
    eng.seq_id      = id;
    eng.step_idx    = 0;
    eng.cur_prio    = def->priority;
    eng.on_done     = on_done;
    eng.in_delay    = false;
    eng.idle        = false;
    eng.reset_phase = RESET_PHASE_NONE;

    ESP_LOGI(TAG, "Play seq %d (prio=%d, steps=%d, reset=%d)",
             id, def->priority, def->count, def->reset_policy);

    advance();
}

static bool can_interrupt(action_priority_t new_prio) {
    if (!eng.seq) return true;
    if (!eng.seq->interruptible) return false;
    if (new_prio >= eng.cur_prio) return true;
    return false;
}

static void handle_play(const engine_msg_t *msg) {
    ;
}

static void handle_play_custom(const engine_msg_t *msg) {
    const action_cmd_t *steps = msg->play_custom.steps;
    uint16_t count            = msg->play_custom.count;
    action_priority_t prio    = msg->play_custom.prio;
    action_done_cb_t on_done  = msg->play_custom.on_done;

    if (!steps || count == 0) return;

    static action_seq_def_t custom_def;
    custom_def.steps          = steps;
    custom_def.count          = count;
    custom_def.priority       = prio;
    custom_def.interruptible  = true;
    custom_def.reset_policy   = RESET_NONE;  /* 自定义序列默认不复位 */
    custom_def.reset_delay_ms = 0;
    custom_def.reset_angle    = 90;

    if (!can_interrupt(prio)) {
        ESP_LOGD(TAG, "Custom seq blocked");
        return;
    }

    if (eng.seq) {
        stop_all_hw();
        finish_sequence(true);
    }

    start_sequence(&custom_def, SEQ_COUNT, on_done);
}

static void handle_stop(void) {
    if (eng.seq) {
        stop_all_hw();
        finish_sequence(true);
    }
}

static void process_msg(const engine_msg_t *msg) {
    switch (msg->cmd) {
        case CMD_PLAY_CUSTOM: handle_play_custom(msg); break;
        case CMD_STOP:        handle_stop();           break;
        default:                break;
    }
}

/* ==================== 引擎任务 ==================== */

static void engine_task(void *arg) {
    (void)arg;
    engine_msg_t msg;

    ESP_LOGI(TAG, "Engine task started");

    for (;;) {
        TickType_t timeout = calc_timeout();

        BaseType_t got = xQueueReceive(eng.queue, &msg, timeout);

        if (got == pdTRUE) {
            process_msg(&msg);
            continue;
        }

        /* 超时：当前延时到期 */
        if (eng.in_delay) {
            on_delay_expired();
        }
    }
}

/* ==================== 公开 API ==================== */

void app_action_engine_init(void) {
    if (eng.task) return;

    memset(&eng, 0, sizeof(eng));
    eng.idle = true;

    g_custom_steps = (action_cmd_t*)heap_caps_malloc(
        CUSTOM_STEPS_MAX * sizeof(action_cmd_t),
        MALLOC_CAP_SPIRAM
    );

    if (!g_custom_steps) {
        ESP_LOGE(TAG, "Failed to alloc custom steps buffer from PSRAM");
        return;
    }

    eng.queue = xQueueCreate(ENGINE_QUEUE_DEPTH, sizeof(engine_msg_t));
    configASSERT(eng.queue);

    BaseType_t ret = xTaskCreate(
        engine_task,
        "act_engine",
        ENGINE_TASK_STACK,
        NULL,
        ENGINE_TASK_PRIO,
        &eng.task
    );
    configASSERT(ret == pdPASS);

    ESP_LOGI(TAG, "Engine init OK");
}

bool app_action_engine_play(sequence_id_t seq_id, action_done_cb_t on_done) {
    if (!eng.queue || seq_id >= SEQ_COUNT) return false;

    engine_msg_t msg = {
        .cmd  = CMD_PLAY,
        .play = { .seq_id = seq_id, .on_done = on_done }
    };
    return xQueueSend(eng.queue, &msg, 0) == pdTRUE;
}

bool app_action_engine_play_custom(const action_cmd_t *steps, uint16_t count,
                               action_priority_t prio, action_done_cb_t on_done) {
    if (!eng.queue || !steps || count == 0) return false;

    engine_msg_t msg = {
        .cmd = CMD_PLAY_CUSTOM,
        .play_custom = {
            .steps   = steps,
            .count   = count,
            .prio    = prio,
            .on_done = on_done,
        }
    };
    return xQueueSend(eng.queue, &msg, 0) == pdTRUE;
}

bool app_action_engine_stop(void) {
    if (!eng.queue) return false;

    engine_msg_t msg = { .cmd = CMD_STOP };
    return xQueueSend(eng.queue, &msg, 0) == pdTRUE;
}

bool app_action_engine_play_from_isr(sequence_id_t seq_id) {
    if (!eng.queue || seq_id >= SEQ_COUNT) return false;

    engine_msg_t msg = {
        .cmd  = CMD_PLAY,
        .play = { .seq_id = seq_id, .on_done = NULL }
    };

    BaseType_t woken = pdFALSE;
    BaseType_t ret = xQueueSendFromISR(eng.queue, &msg, &woken);
    portYIELD_FROM_ISR(woken);
    return ret == pdTRUE;
}

bool app_action_engine_is_idle(void) {
    return eng.idle;
}

const action_cmd_t* app_action_engine_prepare_custom(const action_cmd_t *steps, uint16_t count) {
    if (!g_custom_steps || !steps || count == 0 || count > CUSTOM_STEPS_MAX) {
        return NULL;
    }

    memcpy(g_custom_steps, steps, count * sizeof(action_cmd_t));
    return g_custom_steps;
}