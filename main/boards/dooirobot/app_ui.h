#ifndef __APP_UI_H__
#define __APP_UI_H__

#include <stdbool.h>
#include "lvgl.h"
#include "app_ui_logic.h"
#include "app_periphral.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_UI_TICK_MS 25
#define APP_UI_MS_TO_TICKS(ms) ((ms) / APP_UI_TICK_MS)

#define APP_UI_VERSION "Version:2.0.1"

/* 显示设备枚举 */
typedef enum {
    DISPLAY_LEFT,
    DISPLAY_RIGHT,
    DISPLAY_COUNT
} display_t;

/* 枚举定义不同的GIF动画 */
typedef enum {
    ANIMATION_CLOSE_EYES_SLOW = 0,
    ANIMATION_CLOSE_EYES_QUICK,
    ANIMATION_EXCITED,
    ANIMATION_FEAR,
    ANIMATION_SAD,
    ANIMATION_DISDAIN,
    ANIMATION_LEFT,
    ANIMATION_RIGHT,
    ANIMATION_ANGRY,
    ANIMATION_COUNT 
} gif_animation_type_t;

/* 显示模式定义 */
typedef enum {
    MODE_TIME,  // 常规时间模式
    MODE_TIMER, // 番茄计时模式
    MODE_COUNT,
    MODE_STATS  // 统计模式
} digital_clock_mode_t;

// 摄像头模式定义
typedef enum {
    CAMERA_MODE_NULL = 0,
    CAMERA_MODE_PREVIEW,
    CAMERA_MODE_PHOTO,
    CAMERA_MODE_FACE_DETECT,
    CAMERA_MODE_GESTURE_RECOGNIZE,
    CAMERA_MODE_CLS,               
    CAMERA_MODE_MAX
} camera_mode_t;

/* 摄像头跳转请求：描述进入 SCREEN_CAMERA 时的完整意图 */
typedef enum {
    CAMERA_TRIGGER_NONE = 0,        // 不额外触发，仅切模式
    CAMERA_TRIGGER_FACE_ALIGN_ONCE, // 单次对齐后发 ALIGNED 事件
    CAMERA_TRIGGER_FACE_TRACK_LOOP, // 持续跟随
} camera_trigger_t;

typedef struct {
    camera_mode_t   mode;    // 进入后的初始模式
    camera_trigger_t trigger; // 进入后的触发动作
} camera_request_t;

typedef enum {
    SOUND_1724636,
    SOUND_6812623,
    SOUND_1067002988,
    SOUND_1067936557,
    SOUND_1068702996,
    SOUND_COUNT
} SoundEffect_t;


/* 屏幕类型枚举 */
typedef enum {
    SCREEN_NULL,
    SCREEN_DIGITAL_CLOCK,
    SCREEN_EMOJI_GIF,
    SCREEN_MUSIC_SPECTRUM,
    SCREEN_EYES,
    SCREEN_CAMERA,
    SCREEN_LUCKY_CAT,
    SCREEN_INFO,
    SCREEN_PAGE_SELECT,
    SCREEN_WAKEUP_ACK,
    SCREEN_EMOJI,
    SCREEN_COUNT
} screen_type_t;

/* 屏幕接口 */
typedef struct {
    int (*load)(void);
    int (*update)(void);
    int (*unload)(void);
    void (*on_key_event)(int key_id, touch_btn_event_t event);  
} screen_interface_t;

typedef enum {
    SCREEN_UNLOADED,
    SCREEN_LOADED
} screen_state_t;

/* 屏幕实例 */
typedef struct {
    lv_obj_t *root;
    bool needs_update;
    screen_interface_t *interface;
} screen_instance_t;

/* 按键状态跟踪 */
typedef struct {
    uint32_t left_long_press_time;
    uint32_t right_long_press_time;
} key_state_t;

/* 屏幕管理器 */
typedef struct {
    display_t current_display;
    screen_state_t screen_states[SCREEN_COUNT];
    screen_type_t current_screen;
    screen_type_t old_screen;
    screen_type_t next_screen;
    screen_instance_t screens[DISPLAY_COUNT];
    key_state_t key_state; 
    uint16_t wakeup_ack_timeout_tick;
    uint16_t wakeup_ack_timeout_target_tick;   /* WAKEUP_ACK 超时 tick 数，默认50(1s)，IP显示时500(10s) */
    uint16_t idle_timeout_ticks;   
    bool is_idle_timeout_triggered;
} screen_manager_t;

extern screen_manager_t screen_manager;

void screen_set_current(screen_type_t screen_type);
screen_type_t screen_get_current(void);
void screen_load_next(void);
void screen_ui_control(int key_id, touch_btn_event_t event);
void screen_idle_timeout_reset();
void app_ui_init(void);
uint32_t get_tick_ms(void);

/* 数字时钟 / 番茄钟 */
void digital_clock_mode_set(int mode);
int  digital_clock_mode_get(void);
void digital_timer_set_minutes(int mins);
void digital_timer_data_set(int mins);
void digital_timer_reset(void);

/* 招财猫风格设置（需在 lucky_cat 屏幕中实现） */
void lucky_cat_style_set(int style);

/* 摄像头 */
void camera_set_initial_mode(camera_mode_t mode);
void camera_trigger_funtion(const camera_request_t *req);

void wakeup_ack_set_topic(const char* topic);
void wakeup_ack_add_notification(const char* notification);
void screen_set_wakeup_timeout(int timeout_ticks);
void screen_idle_timeout_trigger(void);

void get_wifi_ip_string(char *buf, size_t buf_size);

void dooi_peripherals_init(screen_type_t screen_type);

/* XIAOZHI SYSTEM */
void ApplicationSendMessage(const char* message, bool use_tool);
void ApplicationPlaySound(SoundEffect_t sound);
void dooi_trigger_chat(void);

void app_codec_gain_set(float gain);

void ui_emoji_set_expression(const char* name);

#ifdef __cplusplus
}
#endif

#endif