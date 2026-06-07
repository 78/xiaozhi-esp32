#include "app_periphral.h"
#include "app_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "app_periphral";

void ir_stop_handler(ir_sensor_id_t sensor_id) {
    app_event_t evt = {.type = APP_EVT_IR_TRIGGER, .data = {.ir_data = {.sensor_id = sensor_id}}};
    app_ui_logic_post(&evt, true);
}

static void on_imu_tilt_evt(imu_tilt_dir_t dir, void* arg) {
    app_event_t evt = {.type = APP_EVT_IMU, .data = {.imu_data = {.dir = dir}}};
    app_ui_logic_post(&evt, false);
}

void on_imu_push_evt(void *arg) {
    if(app_motor_is_active()) {
        return;
    }

    app_event_t evt = {.type = APP_EVT_IMU, .data = {.imu_data = {.dir = IMU_ENV_PUSH}}};
    app_ui_logic_post(&evt, false);
}

static void on_charging_changed(bool is_charging) {
    app_event_t evt = {.type = APP_EVT_POWER_STATUS, .data = {.power_status = {.is_charging = is_charging}}};
    app_ui_logic_post(&evt, true);
}

static void on_low_battery(bool is_low) {
    app_event_t evt = {.type = APP_EVT_POWER_STATUS, .data = {.power_status = {.low_battery = is_low}}};
    app_ui_logic_post(&evt, true);
}

static void on_btn_evt(int key_id, touch_btn_event_t event, void* user_ctx) {
    app_event_t evt = {.type = APP_EVT_BUTTON, .data = {.btn_data = {.btn_id = key_id, .event = event}}};
    app_ui_logic_post(&evt, true);
}

/***************************************************************************************************************/


static void led_off_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    // app_led_off();
    app_led_set_breathing(0x0080FF, 2000, true);
    vTaskDelete(NULL);
}


void app_tip_led_run()
{
    static int8_t led_status = 0, led_tick = 0;
    if(led_tick++ >= APP_UI_MS_TO_TICKS(1000))
    {
        led_tick = 0;
        led_status = !led_status;
        gpio_set_level(52, led_status);
    }
}

void app_periphral_init(void) {
    app_servo_init();
    app_led_init();

    app_imu_init();
    app_imu_set_tilt_callback(on_imu_tilt_evt, NULL);  
    app_imu_enable_tilt_detect(true, 3.0f);
    app_imu_set_push_callback(on_imu_push_evt, NULL);
    app_imu_enable_push_detect(true, 0.3f);

    app_motor_init();

    app_action_engine_init();
    app_motor_set_invert(true, true);

    ir_init(ir_stop_handler);

    static const power_config_t pwr_cfg = {
        .on_charging_changed = on_charging_changed,
        .on_low_battery_changed = on_low_battery,
    };
    power_manager_init(&pwr_cfg);

    static const touch_btn_key_cfg_t keys[] = {
        {.gpio_num = 7, .thresh_ratio = 0.007f},
        {.gpio_num = 8, .thresh_ratio = 0.007f},
    };

    static touch_button_config_t cfg = {
        .keys = keys,
        .key_num = 2,
        .debounce_ms = 30,
        .double_click_ms = 350,
        .long_press_ms = 900,
        .initial_scan_times = 5,
        .oneshot_timeout_ms = 2000,
        .event_queue_len = 32,
        .task_stack = 4096,
        .task_prio = 12,
        .task_core = -1,
        .enable_filter = true,
        .use_fixed_channel_ids = false,
        .fixed_channel_ids = NULL,
    };

    touch_button_handle_t h = NULL;
    ESP_ERROR_CHECK(touch_button_create(&cfg, &h));
    ESP_ERROR_CHECK(touch_button_register_callback(h, TOUCH_BTN_EVT_CLICK, on_btn_evt, NULL));
    ESP_ERROR_CHECK(
        touch_button_register_callback(h, TOUCH_BTN_EVT_DOUBLE_CLICK, on_btn_evt, NULL));
    ESP_ERROR_CHECK(touch_button_register_callback(h, TOUCH_BTN_EVT_LONG_PRESS, on_btn_evt, NULL));
    ESP_ERROR_CHECK(touch_button_start(h));

    // gpio 指示灯
    static const gpio_config_t io_conf = {.pin_bit_mask = (1ULL << 52),
                         .mode = GPIO_MODE_OUTPUT,
                         .pull_up_en = 0,
                         .pull_down_en = 0,
                         .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
}