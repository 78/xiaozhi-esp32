#include "touch_button.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "driver/touch_sens.h"

static const char *TAG = "touch_btn";

// ============================================================
// 内部常量
// ============================================================
#define EXAMPLE_TOUCH_SAMPLE_CFG_NUM        TOUCH_SAMPLE_CFG_NUM  // Up to 'TOUCH_SAMPLE_CFG_NUM'
#define MAX_SUBS_PER_EVENT  2
#define TASK_EXIT_KEY_IDX   0xFF   // 特殊值：通知task退出

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

// ============================================================
// 内部类型
// ============================================================
typedef enum {
    ISR_EVT_ACTIVE = 0,
    ISR_EVT_INACTIVE,
    ISR_EVT_EXIT,           // 用于优雅退出task
} isr_evt_type_t;

typedef struct {
    uint8_t  key_idx;
    uint8_t  type;
    int64_t  ts_us;
} isr_evt_t;

// 修复：timer arg用独立结构体，不再打包指针
typedef struct {
    struct touch_button_t *tb;
    int                    key_idx;
} timer_arg_t;

typedef struct {
    bool     pressed;
    bool     long_fired;
    int64_t  down_ts_us;
    int64_t  last_up_ts_us;
    uint8_t  click_count;

    esp_timer_handle_t long_timer;
    esp_timer_handle_t click_timer;
    timer_arg_t        long_arg;    // timer arg生命周期与key_state一致
    timer_arg_t        click_arg;
} key_state_t;

typedef struct {
    touch_btn_cb_t  cb;
    void           *user_ctx;
} cb_slot_t;

typedef struct touch_button_t {
    touch_button_config_t    cfg;

    touch_sensor_handle_t    sens;
    touch_channel_handle_t  *chans;
    int                     *chan_ids;

    QueueHandle_t            q;
    TaskHandle_t             task;

    key_state_t             *ks;

    cb_slot_t                subs[TOUCH_BTN_EVT_MAX][MAX_SUBS_PER_EVENT];

    bool                     started;
} touch_button_t;

// ============================================================
// 默认值填充（更清晰）
// ============================================================
static void apply_defaults(touch_button_config_t *c)
{
    if (!c->debounce_ms)        c->debounce_ms        = 30;
    if (!c->double_click_ms)    c->double_click_ms    = 350;
    if (!c->long_press_ms)      c->long_press_ms      = 900;
    if (!c->initial_scan_times) c->initial_scan_times = 5;
    if (!c->oneshot_timeout_ms) c->oneshot_timeout_ms = 2000;
    if (!c->event_queue_len)    c->event_queue_len    = 32;
    if (!c->task_stack)         c->task_stack         = 4096;
    if (!c->task_prio)          c->task_prio          = 12;
    // enable_filter：bool默认false，调用方需显式设true（或文档说明）
    // task_core：-1为不绑核，调用方保证
}

// ============================================================
// 事件分发
// ============================================================
static void dispatch_event(touch_button_t *tb, int key_id, touch_btn_event_t evt)
{
    if (evt >= TOUCH_BTN_EVT_MAX) return;
    for (int i = 0; i < MAX_SUBS_PER_EVENT; i++) {
        if (tb->subs[evt][i].cb) {
            tb->subs[evt][i].cb(key_id, evt, tb->subs[evt][i].user_ctx);
        }
    }
}

// ============================================================
// Timer回调（修复：用独立arg结构体）
// ============================================================
static void long_timer_cb(void *arg)
{
    timer_arg_t *ta = (timer_arg_t *)arg;
    touch_button_t *tb = ta->tb;
    int key_idx = ta->key_idx;

    key_state_t *ks = &tb->ks[key_idx];
    if (ks->pressed && !ks->long_fired) {
        ks->long_fired = true;
        dispatch_event(tb, key_idx, TOUCH_BTN_EVT_LONG_PRESS);
    }
}

static void click_timer_cb(void *arg)
{
    timer_arg_t *ta = (timer_arg_t *)arg;
    touch_button_t *tb = ta->tb;
    int key_idx = ta->key_idx;

    key_state_t *ks = &tb->ks[key_idx];
    if (ks->click_count == 1) {
        dispatch_event(tb, key_idx, TOUCH_BTN_EVT_CLICK);
    } else if (ks->click_count >= 2) {
        dispatch_event(tb, key_idx, TOUCH_BTN_EVT_DOUBLE_CLICK);
    }
    ks->click_count = 0;
}

// ============================================================
// ISR回调（提取公共逻辑）
// ============================================================
static int find_key_idx(touch_button_t *tb, int chan_id)
{
    for (int i = 0; i < tb->cfg.key_num; i++) {
        if (tb->chan_ids[i] == chan_id) return i;
    }
    return -1;
}

static bool IRAM_ATTR send_isr_event(touch_button_t *tb, int chan_id, isr_evt_type_t type)
{
    if (!tb || !tb->q) return false;

    int key_idx = find_key_idx(tb, (int)chan_id);
    if (key_idx < 0) return false;

    isr_evt_t e = {
        .key_idx = (uint8_t)key_idx,
        .type    = (uint8_t)type,
        .ts_us   = esp_timer_get_time(),
    };

    BaseType_t hp_woken = pdFALSE;
    xQueueSendFromISR(tb->q, &e, &hp_woken);
    return hp_woken == pdTRUE;
}

static bool IRAM_ATTR on_active_isr(touch_sensor_handle_t sens,
                                     const touch_active_event_data_t *evt, void *ctx)
{
    return send_isr_event((touch_button_t *)ctx, (int)evt->chan_id, ISR_EVT_ACTIVE);
}

static bool IRAM_ATTR on_inactive_isr(touch_sensor_handle_t sens,
                                       const touch_inactive_event_data_t *evt, void *ctx)
{
    return send_isr_event((touch_button_t *)ctx, (int)evt->chan_id, ISR_EVT_INACTIVE);
}

// ============================================================
// 校准
// ============================================================
static esp_err_t calibrate_channels(touch_button_t *tb)
{
    ESP_RETURN_ON_ERROR(touch_sensor_enable(tb->sens), TAG, "enable failed");

    for (int i = 0; i < tb->cfg.initial_scan_times; i++) {
        ESP_RETURN_ON_ERROR(
            touch_sensor_trigger_oneshot_scanning(tb->sens, tb->cfg.oneshot_timeout_ms),
            TAG, "oneshot scan %d failed", i);
    }

    ESP_RETURN_ON_ERROR(touch_sensor_disable(tb->sens), TAG, "disable failed");

    for (int i = 0; i < tb->cfg.key_num; i++) {
        uint32_t bm[TOUCH_SAMPLE_CFG_NUM] = {0};

#if SOC_TOUCH_SUPPORT_BENCHMARK
        touch_chan_data_type_t dtype = TOUCH_CHAN_DATA_TYPE_BENCHMARK;
#else
        touch_chan_data_type_t dtype = TOUCH_CHAN_DATA_TYPE_SMOOTH;
#endif
        ESP_RETURN_ON_ERROR(
            touch_channel_read_data(tb->chans[i], dtype, bm),
            TAG, "read data failed key=%d", i);

        touch_channel_config_t chan_cfg = {0};
#if SOC_TOUCH_SENSOR_VERSION == 1
        for (int j = 0; j < TOUCH_SAMPLE_CFG_NUM; j++) {
            chan_cfg.abs_active_thresh[j] = (uint32_t)(bm[j] * (1.0f - tb->cfg.keys[i].thresh_ratio));
        }
        chan_cfg.charge_speed     = TOUCH_CHARGE_SPEED_7;
        chan_cfg.init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
        chan_cfg.group            = TOUCH_CHAN_TRIG_GROUP_BOTH;
#else
        for (int j = 0; j < EXAMPLE_TOUCH_SAMPLE_CFG_NUM; j++) {
            chan_cfg.active_thresh[j] = (uint32_t)(bm[j] * tb->cfg.keys[i].thresh_ratio);
        }
#endif

        ESP_RETURN_ON_ERROR(
            touch_sensor_reconfig_channel(tb->chans[i], &chan_cfg),
            TAG, "reconfig key=%d failed", i);

        ESP_LOGI(TAG, "Key[%d] gpio=%d chan=%d bm[0]=%"PRIu32" thresh[0]=%"PRIu32,
                 i, tb->cfg.keys[i].gpio_num, tb->chan_ids[i], bm[0],
#if SOC_TOUCH_SENSOR_VERSION == 1
                 chan_cfg.abs_active_thresh[0]);
#else
                 chan_cfg.active_thresh[0]);
#endif
    }
    return ESP_OK;
}

// ============================================================
// Channel创建（提取公共初始config）
// ============================================================
static void fill_channel_config(touch_channel_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
#if SOC_TOUCH_SENSOR_VERSION == 3
    cfg->active_thresh[0] = 1000;
    cfg->active_thresh[1] = 2500;
//    cfg->active_thresh[2] = 5000;
#elif SOC_TOUCH_SENSOR_VERSION == 2
    cfg->active_thresh[0] = 2000;
#else
    cfg->abs_active_thresh[0] = 1000;
    cfg->charge_speed          = TOUCH_CHARGE_SPEED_7;
    cfg->init_charge_volt      = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
    cfg->group                 = TOUCH_CHAN_TRIG_GROUP_BOTH;
#endif
}

static esp_err_t create_channels(touch_button_t *tb)
{
    const int n = tb->cfg.key_num;

    // 自动探测
    bool found[n];  // VLA or use fixed max
    memset(found, 0, sizeof(bool) * n);

    touch_channel_config_t chan_cfg;
    fill_channel_config(&chan_cfg);

    for (int cid = (TOUCH_MIN_CHAN_ID + 5); cid <= (TOUCH_MIN_CHAN_ID + 6); cid++) {
        touch_channel_handle_t tmp = NULL;
        if (touch_sensor_new_channel(tb->sens, cid, &chan_cfg, &tmp) != ESP_OK) continue;

        touch_chan_info_t info = {0};
        if (touch_sensor_get_channel_info(tmp, &info) != ESP_OK) {
            touch_sensor_del_channel(tmp);
            continue;
        }

        bool matched = false;
        for (int k = 0; k < n; k++) {
            if (!found[k] && info.chan_gpio == tb->cfg.keys[k].gpio_num) {
                tb->chans[k]   = tmp;
                tb->chan_ids[k] = cid;
                found[k]       = true;
                matched = true;
                ESP_LOGI(TAG, "Key[%d] gpio=%d -> chan=%d", k, tb->cfg.keys[k].gpio_num, cid);
                break;
            }
        }

        if (!matched) {
            touch_sensor_del_channel(tmp);
        }
    }

    for (int k = 0; k < n; k++) {
        if (!found[k]) {
            ESP_LOGE(TAG, "gpio=%d (key=%d) no channel found", tb->cfg.keys[k].gpio_num, k);
            return ESP_ERR_NOT_FOUND;
        }
    }
    return ESP_OK;
}

// ============================================================
// Worker task（优化：优雅退出 + 逻辑整理）
// ============================================================
static void handle_press(touch_button_t *tb, int idx, int64_t now)
{
    key_state_t *ks = &tb->ks[idx];
    const int64_t debounce_us = (int64_t)tb->cfg.debounce_ms * 1000;

    if (ks->pressed) return;
    if (ks->last_up_ts_us > 0 && (now - ks->last_up_ts_us) < debounce_us) return;

    ks->pressed    = true;
    ks->long_fired = false;
    ks->down_ts_us = now;

    dispatch_event(tb, idx, TOUCH_BTN_EVT_DOWN);

    esp_timer_stop(ks->long_timer);
    esp_timer_start_once(ks->long_timer, (uint64_t)tb->cfg.long_press_ms * 1000);
}

static void handle_release(touch_button_t *tb, int idx, int64_t now)
{
    key_state_t *ks = &tb->ks[idx];
    const int64_t debounce_us = (int64_t)tb->cfg.debounce_ms * 1000;

    if (!ks->pressed) return;

    // 去抖：按下时间太短认为是noise
    if (ks->down_ts_us > 0 && (now - ks->down_ts_us) < debounce_us) {
        ks->pressed        = false;
        ks->last_up_ts_us  = now;
        esp_timer_stop(ks->long_timer);
        return;
    }

    ks->pressed       = false;
    ks->last_up_ts_us = now;
    esp_timer_stop(ks->long_timer);

    dispatch_event(tb, idx, TOUCH_BTN_EVT_UP);

    if (ks->long_fired) {
        dispatch_event(tb, idx, TOUCH_BTN_EVT_LONG_RELEASE);
        ks->click_count = 0;
        esp_timer_stop(ks->click_timer);
    } else {
        ks->click_count++;
        esp_timer_stop(ks->click_timer);
        esp_timer_start_once(ks->click_timer, (uint64_t)tb->cfg.double_click_ms * 1000);
    }
}

static void touch_btn_task(void *arg)
{
    touch_button_t *tb = (touch_button_t *)arg;
    isr_evt_t e;

    while (xQueueReceive(tb->q, &e, portMAX_DELAY) == pdTRUE) {
        if (e.type == ISR_EVT_EXIT) break;
        if (e.key_idx >= (uint8_t)tb->cfg.key_num) continue;

        if (e.type == ISR_EVT_ACTIVE) {
            handle_press(tb, e.key_idx, e.ts_us);
        } else {
            handle_release(tb, e.key_idx, e.ts_us);
        }
    }

    vTaskDelete(NULL);
}

// ============================================================
// 公共 API
// ============================================================
esp_err_t touch_button_register_callback(touch_button_handle_t handle,
                                          touch_btn_event_t event,
                                          touch_btn_cb_t cb,
                                          void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle && cb, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(event < TOUCH_BTN_EVT_MAX, ESP_ERR_INVALID_ARG, TAG, "bad event");

    touch_button_t *tb = (touch_button_t *)handle;
    for (int i = 0; i < MAX_SUBS_PER_EVENT; i++) {
        if (!tb->subs[event][i].cb) {
            tb->subs[event][i].cb       = cb;
            tb->subs[event][i].user_ctx = user_ctx;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t touch_button_create(const touch_button_config_t *cfg, touch_button_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(cfg && out_handle && cfg->keys, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(cfg->key_num > 0, ESP_ERR_INVALID_ARG, TAG, "key_num must > 0");

    touch_button_t *tb = calloc(1, sizeof(touch_button_t));
    ESP_RETURN_ON_FALSE(tb, ESP_ERR_NO_MEM, TAG, "no mem for tb");

    tb->cfg = *cfg;
    apply_defaults(&tb->cfg);

    tb->chans    = calloc(tb->cfg.key_num, sizeof(touch_channel_handle_t));
    tb->chan_ids = calloc(tb->cfg.key_num, sizeof(int));
    tb->ks       = calloc(tb->cfg.key_num, sizeof(key_state_t));
    if (!tb->chans || !tb->chan_ids || !tb->ks) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    for (int i = 0; i < tb->cfg.key_num; i++) tb->chan_ids[i] = -1;

    tb->q = xQueueCreate(tb->cfg.event_queue_len, sizeof(isr_evt_t));
    if (!tb->q) { ret = ESP_ERR_NO_MEM; goto fail; }

    // 1) sensor controller
    touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
#if SOC_TOUCH_SENSOR_VERSION == 3
        TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG2(3, 29, 8, 3),
        TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG2(2, 88, 31, 7),
        TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG2(3, 10, 31, 7),
#elif SOC_TOUCH_SENSOR_VERSION == 2
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V2),
#else
        TOUCH_SENSOR_V1_DEFAULT_SAMPLE_CONFIG(5.0, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_1V7),
#endif
    };

#if SOC_TOUCH_SENSOR_VERSION == 3
    const int sample_num = EXAMPLE_TOUCH_SAMPLE_CFG_NUM;
#else
    const int sample_num = 1;
#endif

    touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(sample_num, sample_cfg);
    ESP_GOTO_ON_ERROR(touch_sensor_new_controller(&sens_cfg, &tb->sens), fail, TAG, "new_controller failed");

    // 2) channels
    ESP_GOTO_ON_ERROR(create_channels(tb), fail, TAG, "create channels failed");

    // 3) filter
    if (tb->cfg.enable_filter) {
        touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
        ESP_GOTO_ON_ERROR(touch_sensor_config_filter(tb->sens, &filter_cfg), fail, TAG, "config filter failed");
    }

    // 4) calibration
    ESP_GOTO_ON_ERROR(calibrate_channels(tb), fail, TAG, "calibrate failed");

    // 5) ISR callbacks
    touch_event_callbacks_t cbs = {
        .on_active   = on_active_isr,
        .on_inactive = on_inactive_isr,
    };
    ESP_GOTO_ON_ERROR(touch_sensor_register_callbacks(tb->sens, &cbs, tb), fail, TAG, "register cbs failed");

    // 6) per-key timers（使用独立arg结构体，彻底解决指针打包问题）
    for (int i = 0; i < tb->cfg.key_num; i++) {
        key_state_t *ks = &tb->ks[i];

        ks->long_arg  = (timer_arg_t){ .tb = tb, .key_idx = i };
        ks->click_arg = (timer_arg_t){ .tb = tb, .key_idx = i };

        esp_timer_create_args_t ta = {
            .dispatch_method       = ESP_TIMER_TASK,
            .skip_unhandled_events = true,
        };

        ta.callback = long_timer_cb;
        ta.arg      = &ks->long_arg;
        ta.name     = "tb_long";
        ESP_GOTO_ON_ERROR(esp_timer_create(&ta, &ks->long_timer), fail, TAG, "long timer key=%d", i);

        ta.callback = click_timer_cb;
        ta.arg      = &ks->click_arg;
        ta.name     = "tb_click";
        ESP_GOTO_ON_ERROR(esp_timer_create(&ta, &ks->click_timer), fail, TAG, "click timer key=%d", i);
    }

    // 7) worker task
    BaseType_t ok;
    if (tb->cfg.task_core >= 0) {
        ok = xTaskCreatePinnedToCore(touch_btn_task, "touch_btn", tb->cfg.task_stack,
                                     tb, tb->cfg.task_prio, &tb->task, tb->cfg.task_core);
    } else {
        ok = xTaskCreate(touch_btn_task, "touch_btn", tb->cfg.task_stack,
                         tb, tb->cfg.task_prio, &tb->task);
    }
    if (ok != pdPASS) { ret = ESP_ERR_NO_MEM; goto fail; }

    *out_handle = (touch_button_handle_t)tb;
    return ESP_OK;

fail:
    touch_button_destroy((touch_button_handle_t)tb);
    return ret;   // 返回实际错误码，不再固定ESP_FAIL
}

esp_err_t touch_button_start(touch_button_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    touch_button_t *tb = (touch_button_t *)handle;
    if (tb->started) return ESP_OK;

    ESP_RETURN_ON_ERROR(touch_sensor_enable(tb->sens), TAG, "enable failed");
    ESP_RETURN_ON_ERROR(touch_sensor_start_continuous_scanning(tb->sens), TAG, "start scan failed");

    tb->started = true;
    return ESP_OK;
}

esp_err_t touch_button_stop(touch_button_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    touch_button_t *tb = (touch_button_t *)handle;
    if (!tb->started) return ESP_OK;

    if (tb->ks) {
        for (int i = 0; i < tb->cfg.key_num; i++) {
            esp_timer_stop(tb->ks[i].long_timer);
            esp_timer_stop(tb->ks[i].click_timer);
        }
    }

    touch_sensor_stop_continuous_scanning(tb->sens);
    touch_sensor_disable(tb->sens);

    tb->started = false;
    return ESP_OK;
}

esp_err_t touch_button_destroy(touch_button_handle_t handle)
{
    if (!handle) return ESP_OK;
    touch_button_t *tb = (touch_button_t *)handle;

    touch_button_stop(handle);

    // 优雅退出task：发送EXIT事件，等task自己退出
    if (tb->task && tb->q) {
        isr_evt_t exit_evt = { .type = ISR_EVT_EXIT };
        xQueueSend(tb->q, &exit_evt, pdMS_TO_TICKS(100));
        // task会自己vTaskDelete，这里只是解除引用
        tb->task = NULL;
    }

    if (tb->ks) {
        for (int i = 0; i < tb->cfg.key_num; i++) {
            if (tb->ks[i].long_timer)  esp_timer_delete(tb->ks[i].long_timer);
            if (tb->ks[i].click_timer) esp_timer_delete(tb->ks[i].click_timer);
        }
    }

    if (tb->chans) {
        for (int i = 0; i < tb->cfg.key_num; i++) {
            if (tb->chans[i]) touch_sensor_del_channel(tb->chans[i]);
        }
    }

    if (tb->sens) touch_sensor_del_controller(tb->sens);

    if (tb->q) vQueueDelete(tb->q);

    free(tb->chans);
    free(tb->chan_ids);
    free(tb->ks);
    free(tb);
    return ESP_OK;
}
