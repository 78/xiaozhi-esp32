#include "app_ui_logic.h"
#include "app_ui.h"
#include "app_periphral.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ui_logic";

static QueueHandle_t s_evt_queue = NULL;

app_system_state_t g_app_state = {
    .current_state = APP_EVT_STATE_LOADING_PROTOCOL,
    .prev_state    = APP_EVT_STATE_LOADING_PROTOCOL,
};

typedef void (*app_event_handler_t)(const app_event_t *evt);

/* -----------------------------------------------------------------------
 * 辅助：判断某状态是否属于"对话中"（LISTENING 或 SPEAKING）
 * --------------------------------------------------------------------- */
static inline bool is_conv_state()
{
    return g_app_state.is_conv_state;
}

static void set_conv_state(app_event_type_t new_state)
{
    if (new_state == g_app_state.current_state) return;
    g_app_state.prev_state    = g_app_state.current_state;
    g_app_state.current_state = new_state;
}

/* -----------------------------------------------------------------------
 * 系统 / 对话状态处理
 * --------------------------------------------------------------------- */
static void on_state_loading_protocol(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_LOADING_PROTOCOL);
}

static void on_state_connect_wifi(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_CONNECT_WIFI);
}

static void on_state_standby(const app_event_t *evt)
{
    app_servo_set_target_angle(SERVO_TARGET_BOTH, 180, 2000);
    /* 若是从对话状态退出，关闭呼吸灯 */
    if (is_conv_state()) {
        g_app_state.is_conv_state = false;
        app_led_off();
    }
    set_conv_state(APP_EVT_STATE_STANDBY);
}

static void on_state_listening(const app_event_t *evt)
{
    // FIXME: 这里不太合理
    if(screen_get_current() != SCREEN_CAMERA)
    {
        app_event_t evt_to_post = {.type = APP_EVT_VISION_TASK_EXITED};
        app_ui_logic_post(&evt_to_post, false);
    }

    app_imu_enable_tilt_detect(false, 5.0f);
    
    screen_idle_timeout_reset();

    /* 首次进入对话（前一状态不是对话态）→ 开启呼吸灯 */
    if (!is_conv_state()) {
        ESP_LOGI(TAG, "Conversation started → LED BREATHING");
        app_led_set_breathing(0x0080FF, 2000, true);
        app_servo_set_target_angle(SERVO_TARGET_BOTH, 135, 1500);
    }

    g_app_state.is_conv_state = true;
    set_conv_state(APP_EVT_STATE_LISTENING);
}

static void on_state_speaking(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_SPEAKING);
}

static void on_state_connecting(const app_event_t *evt)
{
    app_event_t evt_to_post = {.type = APP_EVT_VISION_TASK_EXITED};
    app_ui_logic_post(&evt_to_post, false);
    
    set_conv_state(APP_EVT_STATE_CONNECTING);
}


static void on_state_activation(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_ACTIVATION);
}

static void on_state_checking_new_version(const app_event_t *evt)
{
    if (!g_app_state.sys_is_init_done) {
        g_app_state.sys_is_init_done = true;
        screen_set_current(SCREEN_EMOJI);
    }
}

static void on_state_error(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_ERROR);
}

static void on_state_idle(const app_event_t *evt)
{
    set_conv_state(APP_EVT_STATE_IDLE);
}

static void on_bluetooth_open(const app_event_t *evt)
{
    app_event_t evt_to_post = {.type = APP_EVT_VISION_TASK_EXITED};
    app_ui_logic_post(&evt_to_post, false);
}

static void on_bluetooth_close(const app_event_t *evt)
{
    ;
}

static void on_web_server_connect(const app_event_t *evt)
{
    app_event_t evt_to_post = {.type = APP_EVT_VISION_TASK_EXITED};
    app_ui_logic_post(&evt_to_post, false);

    evt_to_post.type = APP_EVT_IR_ENABLE;
    app_ui_logic_post(&evt_to_post, false);
}

static void on_web_server_disconnect(const app_event_t *evt)
{
    app_event_t evt_to_post = {.type = APP_EVT_IR_DISABLE};
    app_ui_logic_post(&evt_to_post, false);
}

/* -----------------------------------------------------------------------
 * 外设 / 硬件事件处理
 * --------------------------------------------------------------------- */
static void on_camera_trigger_preview(const app_event_t *evt)
{
    ;
}

static void on_ir_enable(const app_event_t *evt)
{
    ir_enable();
    vTaskDelay(pdMS_TO_TICKS(20));
    ir_unmute();
}

static void on_ir_disable(const app_event_t *evt)
{
    ir_mute();
    ir_disable();
}

static void on_ir_trigger(const app_event_t *evt)
{
    if (app_motor_is_active()) {
        ApplicationSendMessage("检测到机身悬空", false);
        app_motor_estop();
        if (evt->data.ir_data.sensor_id == IR_SENSOR_3 ||
            evt->data.ir_data.sensor_id == IR_SENSOR_4) {
            app_motor_move_auto(-100, -100, 200);
        } else {
            app_motor_move_auto(100, 100, 200);
        }
    }
    ESP_LOGW(TAG, "IR Triggered: %d\n", evt->data.ir_data.sensor_id);
}

static void on_btn_evt(const app_event_t *evt)
{
    int key_id                = evt->data.btn_data.btn_id;
    touch_btn_event_t event   = evt->data.btn_data.event;
    screen_ui_control(key_id, event);

    const char *ename = (event == TOUCH_BTN_EVT_CLICK)         ? "CLICK"
                      : (event == TOUCH_BTN_EVT_DOUBLE_CLICK)  ? "DBL"
                      : (event == TOUCH_BTN_EVT_LONG_PRESS)    ? "LONG"
                                                               : "?";
    ESP_LOGI(TAG, "key=%d evt=%s", key_id, ename);
}

static void on_imu(const app_event_t *evt)
{
    static uint8_t imu_push_count = 0;
    static uint32_t first_push_tick = 0;

    imu_tilt_dir_t dir = evt->data.imu_data.dir;
    ESP_LOGW(TAG, "IMU Tilt Detected! Dir: %d", dir);

    if (app_motor_is_active()) {
        return;
    }

    switch (dir) {
    case IMU_TILT_RIGHT:   // 正前方
        ApplicationSendMessage("你好呀", false);
        break;
    case IMU_TILT_FRONT:   // 右边
        app_servo_set_angle(SERVO_TARGET_RIGHT, 180.0f, 1000);
        break;
    case IMU_TILT_BACK:    // 左边
        app_servo_set_angle(SERVO_TARGET_LEFT, 180.0f, 1000);
        break;
    case IMU_ENV_PUSH: {
        uint32_t now = get_tick_ms();

        if (imu_push_count == 0) {
            first_push_tick = now;
            imu_push_count = 1;
            ApplicationPlaySound(SOUND_1724636);
        } else {
            if ((now - first_push_tick) <= 6000) {
                imu_push_count++;

                if (imu_push_count >= 3) {
                    ApplicationSendMessage("哎呦你在推我吗", false);
                    imu_push_count = 0;
                    first_push_tick = 0;
                } else {
                    ApplicationPlaySound(SOUND_6812623);
                }
            } else {
                // 超过6秒，重新开始计数
                first_push_tick = now;
                imu_push_count = 1;
                ApplicationPlaySound(SOUND_6812623);
            }
        }
        break;
    }

    default:
        break;
    }
}

static void on_power_status(const app_event_t *evt)
{
    bool is_low      = evt->data.power_status.low_battery;
    bool is_charging = evt->data.power_status.is_charging;

    if (is_low && !is_charging){
        ApplicationSendMessage("电池电量低", false);
        ESP_LOGW(TAG, "Low battery!");
    }
}

static void on_vision_task_exited(const app_event_t *evt)
{
    ;
}

static void on_vision_task_started(const app_event_t *evt)
{
    ;
}

static void on_face_detected(const app_event_t *evt)
{
    ;
}

static void on_face_aligned(const app_event_t *evt)
{
    ESP_LOGI(TAG, "Face Aligned!");
    screen_idle_timeout_reset();
}

static void on_gesture(const app_event_t *evt)
{
    ;
}

static void on_object_event(const app_event_t *evt)
{
    ;
}


/* -----------------------------------------------------------------------
 * 事件分发表
 * --------------------------------------------------------------------- */
static const app_event_handler_t s_handler_table[APP_EVT_COUNT] = {
    [APP_EVT_STATE_LOADING_PROTOCOL]     = on_state_loading_protocol,
    [APP_EVT_STATE_CONNECT_WIFI]         = on_state_connect_wifi,
    [APP_EVT_STATE_STANDBY]              = on_state_standby,
    [APP_EVT_STATE_LISTENING]            = on_state_listening,
    [APP_EVT_STATE_SPEAKING]             = on_state_speaking,
    [APP_EVT_STATE_CONNECTING]           = on_state_connecting,
    [APP_EVT_STATE_ACTIVATION]           = on_state_activation,
    [APP_EVT_STATE_CHECKING_NEW_VERSION] = on_state_checking_new_version,
    [APP_EVT_STATE_ERROR]                = on_state_error,
    [APP_EVT_STATE_IDLE]                 = on_state_idle,

    [APP_EVT_SHOW_NOTIFICATION]          = NULL,
    [APP_EVT_CHAT_MESSAGE]               = NULL,

    [APP_EVT_SCREEN_CHANGE]              = NULL,
    [APP_EVT_SCREEN_NEXT]                = NULL,

    [APP_EVT_BLUETOOTH_OPEN]             = on_bluetooth_open,
    [APP_EVT_BLUETOOTH_CLOSE]            = on_bluetooth_close,
    [APP_EVT_WEB_SERVER_CONNECT]         = on_web_server_connect,
    [APP_EVT_WEB_SERVER_DISCONNECT]      = on_web_server_disconnect,

    [APP_EVT_VISION_TASK_EXITED]         = on_vision_task_exited,
    [APP_EVT_VISION_TASK_STARTED]        = on_vision_task_started,
    [APP_EVT_CAMERA_TIGGRER_PREVIEW]     = on_camera_trigger_preview,
    [APP_EVT_IR_ENABLE]                  = on_ir_enable,
    [APP_EVT_IR_DISABLE]                 = on_ir_disable,
    [APP_EVT_IR_TRIGGER]                 = on_ir_trigger,
    [APP_EVT_BUTTON]                     = on_btn_evt,
    [APP_EVT_IMU]                        = on_imu,
    [APP_EVT_POWER_STATUS]               = on_power_status,

    [APP_EVT_FACE_DETECTED]              = on_face_detected,
    [APP_EVT_FACE_ALIGNED]               = on_face_aligned,            // 人脸对齐
    [APP_EVT_GESTURE]                    = on_gesture,
    [APP_EVT_OBJECT]                     = on_object_event,            // 物体识别结果
};

/* -----------------------------------------------------------------------
 * 逻辑任务
 * --------------------------------------------------------------------- */
static void logic_task(void *arg)
{
    app_event_t evt;
    while (1) {
        if (xQueueReceive(s_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt.type < APP_EVT_COUNT && s_handler_table[evt.type]) {
                s_handler_table[evt.type](&evt);
            } else {
                ESP_LOGW(TAG, "Unhandled event type: %d", evt.type);
            }
        }
    }
}

bool app_ui_logic_post(const app_event_t *evt, bool from_isr)
{
    if (!s_evt_queue) return false;
    if (from_isr) {
        BaseType_t woken = pdFALSE;
        bool ok = xQueueSendFromISR(s_evt_queue, evt, &woken) == pdTRUE;
        portYIELD_FROM_ISR(woken);
        return ok;
    }
    return xQueueSend(s_evt_queue, evt, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool app_ui_logic_is_init_done(void)
{
    return g_app_state.sys_is_init_done;
}

app_event_type_t app_ui_logic_get_state(void)
{
    return g_app_state.current_state;
}

void app_ui_logic_init(void)
{
    s_evt_queue = xQueueCreate(LOGIC_QUEUE_DEPTH, sizeof(app_event_t));
    configASSERT(s_evt_queue);

    xTaskCreatePinnedToCore(
        logic_task, "ui_logic",
        LOGIC_TASK_STACK, NULL,
        LOGIC_TASK_PRIORITY, NULL,
        1
    );
    ESP_LOGI(TAG, "app_ui_logic initialized");
}