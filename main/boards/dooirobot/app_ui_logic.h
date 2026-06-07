#ifndef __APP_UI_LOGIC_H__
#define __APP_UI_LOGIC_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_ui.h"
#include "app_periphral.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGIC_QUEUE_DEPTH     5
#define LOGIC_TASK_PRIORITY   10
#define LOGIC_TASK_STACK      4096

typedef enum {
    APP_EVT_NONE,

    /* ── 对话 / 系统状态事件 ── */
    APP_EVT_STATE_LOADING_PROTOCOL,      // 开机加载协议
    APP_EVT_STATE_CONNECT_WIFI,          // 连接 WiFi
    APP_EVT_STATE_STANDBY,               // 待命
    APP_EVT_STATE_LISTENING,             // 监听
    APP_EVT_STATE_SPEAKING,              // 说话
    APP_EVT_STATE_CONNECTING,            // 连接中
    APP_EVT_STATE_ACTIVATION,            // 激活
    APP_EVT_STATE_CHECKING_NEW_VERSION,  // 检查新版本
    APP_EVT_STATE_ERROR,                 // 错误
    APP_EVT_STATE_IDLE,                  // 待机

    /* ── 显示事件 ── */
    APP_EVT_SHOW_NOTIFICATION,           // 显示通知气泡
    APP_EVT_CHAT_MESSAGE,                // 显示对话消息

    /* ── 屏幕 / UI 事件 ── */
    APP_EVT_SCREEN_CHANGE,               // 主动切屏
    APP_EVT_SCREEN_NEXT,                 // 翻页

    /* ── 系统 / 连接事件 ── */
    APP_EVT_BLUETOOTH_OPEN,
    APP_EVT_BLUETOOTH_CLOSE,
    APP_EVT_WEB_SERVER_CONNECT,
    APP_EVT_WEB_SERVER_DISCONNECT,

    APP_EVT_CAMERA_TIGGRER_PREVIEW,
    APP_EVT_IR_ENABLE,
    APP_EVT_IR_DISABLE,
    APP_EVT_IR_TRIGGER,
    APP_EVT_BUTTON,
    APP_EVT_IMU,
    APP_EVT_POWER_STATUS,

    APP_EVT_VISION_TASK_EXITED,         // 视觉任务退出
    APP_EVT_VISION_TASK_STARTED,        // 视觉任务启动
    APP_EVT_FACE_DETECTED,              // 人脸检测到
    APP_EVT_FACE_ALIGNED,               // 人脸对齐完成
    APP_EVT_GESTURE,                    // 手势识别结果
    APP_EVT_OBJECT,                     // 物体识别结果

    APP_EVT_COUNT
} app_event_type_t;

typedef union {
    struct {
        ir_sensor_id_t sensor_id;
    } ir_data;

    struct {
        int16_t           btn_id;
        touch_btn_event_t event;
    } btn_data;

    struct {
        imu_tilt_dir_t dir;
    } imu_data;

    struct {
        bool is_charging;
        bool low_battery;
    } power_status;

    struct {
        int reverse;
    } reverse;

} app_event_data_t;

typedef struct {
    app_event_type_t  type;
    app_event_data_t  data;
} app_event_t;

typedef struct {
    app_event_type_t current_state;     // 当前对话状态
    app_event_type_t prev_state;        // 上一个对话状态
    bool sys_is_init_done;
    bool is_conv_state;                 // 是否处于对话状态（LISTENING 或 SPEAKING）
} app_system_state_t;

/**
 * @brief 初始化逻辑层，创建事件队列和逻辑任务
 */
void app_ui_logic_init(void);

/**
 * @brief 向逻辑层投递事件（线程安全，可从任意任务/ISR 调用）
 * @param evt      事件指针
 * @param from_isr 是否来自中断上下文
 * @return true 投递成功
 */
bool app_ui_logic_post(const app_event_t *evt, bool from_isr);

/**
 * @brief 查询当前对话状态
 */
app_event_type_t app_ui_logic_get_state(void);

bool app_ui_logic_is_init_done(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_LOGIC_H__ */