#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOUCH_BTN_KEY_LEFT = 0,
    TOUCH_BTN_KEY_RIGHT,
    TOUCH_BTN_KEY_MAX, 
} touch_btn_key_id_t;

typedef struct touch_button_t *touch_button_handle_t;

typedef enum {
    TOUCH_BTN_EVT_DOWN = 0,
    TOUCH_BTN_EVT_UP,
    TOUCH_BTN_EVT_CLICK,
    TOUCH_BTN_EVT_DOUBLE_CLICK,
    TOUCH_BTN_EVT_LONG_PRESS,
    TOUCH_BTN_EVT_LONG_RELEASE,
    TOUCH_BTN_EVT_MAX,  // 新增：用于数组边界，避免硬编码
} touch_btn_event_t;

typedef void (*touch_btn_cb_t)(int key_id, touch_btn_event_t event, void *user_ctx);

typedef struct {
    int      gpio_num;
    float    thresh_ratio;   // 建议 0.01~0.03 (1%~3%)
} touch_btn_key_cfg_t;

typedef struct {
    const touch_btn_key_cfg_t *keys;
    int      key_num;

    // timing (ms)，0表示使用默认值
    uint32_t debounce_ms;
    uint32_t double_click_ms;
    uint32_t long_press_ms;

    // calibration
    int      initial_scan_times;
    uint32_t oneshot_timeout_ms;

    // task/queue
    int      event_queue_len;
    uint32_t task_stack;
    int      task_prio;
    int      task_core;        // -1 = 不绑核

    bool     enable_filter;    // 推荐 true

    // 固定channel模式（跳过自动探测）
    bool     use_fixed_channel_ids;
    const int *fixed_channel_ids;
} touch_button_config_t;

// 生命周期
esp_err_t touch_button_create(const touch_button_config_t *cfg,
                               touch_button_handle_t *out_handle);
esp_err_t touch_button_start(touch_button_handle_t handle);
esp_err_t touch_button_stop(touch_button_handle_t handle);
esp_err_t touch_button_destroy(touch_button_handle_t handle);

// 回调注册（每个事件最多 MAX_SUBS_PER_EVENT 个）
esp_err_t touch_button_register_callback(touch_button_handle_t handle,
                                          touch_btn_event_t event,
                                          touch_btn_cb_t cb,
                                          void *user_ctx);

#ifdef __cplusplus
}
#endif
