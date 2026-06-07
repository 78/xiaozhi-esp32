#include "app_ui.h"
#include "display_hal.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_ui_logic.h"

static const char *TAG = "app_ui";

#define DUAL_LONG_PRESS_WINDOW_MS  1000
#define IDLE_TIMEOUT_MS            300000
// #define IDLE_TIMEOUT_MS            60000

/* 屏幕管理器实例 */
screen_manager_t screen_manager = {0};

/* 外部屏幕接口声明 */
extern screen_interface_t wakeup_ack_screen_interface;
extern screen_interface_t emoji_screen_interface;

static screen_interface_t *screen_interfaces[SCREEN_COUNT] = {
    [SCREEN_DIGITAL_CLOCK] = NULL,
    [SCREEN_EMOJI_GIF]     = NULL,
    [SCREEN_MUSIC_SPECTRUM]= NULL,
    [SCREEN_EYES]          = NULL,
    [SCREEN_CAMERA]        = NULL,
    [SCREEN_LUCKY_CAT]     = NULL,
    [SCREEN_INFO]          = NULL,
    [SCREEN_PAGE_SELECT]   = NULL,
    [SCREEN_WAKEUP_ACK]    = &wakeup_ack_screen_interface,
    [SCREEN_EMOJI]         = &emoji_screen_interface,
};

uint32_t get_tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool is_direct_render_screen(screen_type_t id)
{
    return (id == SCREEN_EYES);
}

void screen_switch_display(display_t display_id)
{
    display_dev_t *dev = display_get_dev(display_id);
    lv_disp_set_default(dev->lv_disp);
    screen_manager.current_display = display_id;
}

void screen_set_current(screen_type_t screen_type)
{
    if (screen_type == SCREEN_WAKEUP_ACK) {
        screen_manager.wakeup_ack_timeout_tick = 0;
        screen_manager.wakeup_ack_timeout_target_tick = APP_UI_MS_TO_TICKS(1500);
    }
    screen_manager.next_screen = screen_type;
}

screen_type_t screen_get_current(void)
{
    return screen_manager.current_screen;
}

void screen_set_wakeup_timeout(int timeout_ticks)
{
    screen_manager.wakeup_ack_timeout_target_tick = timeout_ticks;
}

void screen_init(void)
{
    screen_switch_display(DISPLAY_LEFT);
    screen_manager.screens[DISPLAY_LEFT].root = lv_obj_create(NULL);
    screen_switch_display(DISPLAY_RIGHT);
    screen_manager.screens[DISPLAY_RIGHT].root = lv_obj_create(NULL);
}

void screen_load_next(void)
{
    int screen_type = screen_manager.current_screen;

    do {
        screen_type++;
        if (screen_type >= SCREEN_COUNT) {
            screen_type = SCREEN_DIGITAL_CLOCK;
        }
    } while (screen_interfaces[screen_type] == NULL && screen_type != SCREEN_DIGITAL_CLOCK);

    screen_set_current(screen_type);
}

void screen_load(screen_type_t screen_type)
{
    int current_screen = screen_manager.current_screen;

    if (screen_type >= SCREEN_COUNT || screen_type == SCREEN_NULL) {
        return;
    }

    if (current_screen != SCREEN_NULL && screen_interfaces[current_screen] != NULL) {
        screen_manager.screen_states[current_screen] = SCREEN_UNLOADED;
        ESP_LOGI(TAG, "unload screen %d", current_screen);
        screen_interfaces[current_screen]->unload();
        if (screen_manager.screens[DISPLAY_LEFT].root) {
            lv_obj_clean(screen_manager.screens[DISPLAY_LEFT].root); 
        }
        if (screen_manager.screens[DISPLAY_RIGHT].root) {
            lv_obj_clean(screen_manager.screens[DISPLAY_RIGHT].root);
        }
    }

    ESP_LOGI(TAG, "load screen %d", screen_type);

    if (screen_interfaces[screen_type] != NULL) {
        screen_interfaces[screen_type]->load();
    }

    screen_manager.screen_states[screen_type] = SCREEN_LOADED;

    if (!is_direct_render_screen(screen_type)) {
        if (screen_manager.screens[DISPLAY_LEFT].root != NULL) {
            lv_scr_load(screen_manager.screens[DISPLAY_LEFT].root);
        }
        if (screen_manager.screens[DISPLAY_RIGHT].root != NULL) {
            lv_scr_load(screen_manager.screens[DISPLAY_RIGHT].root);
        }
        lv_timer_handler();
    }
}

void screen_update(void)
{
    if (screen_manager.next_screen != SCREEN_NULL) {
        if (screen_manager.current_screen != screen_manager.next_screen) {
            screen_manager.old_screen = screen_manager.current_screen;
            screen_load(screen_manager.next_screen);
            screen_manager.current_screen = screen_manager.next_screen;
            dooi_peripherals_init(screen_manager.next_screen);
        }
        screen_manager.next_screen = SCREEN_NULL;
    }

    int current_screen = screen_manager.current_screen;
    screen_interface_t *screen = screen_interfaces[current_screen];

    if (!screen || !screen->update) {
        return;
    }

    if (screen_manager.screen_states[current_screen] == SCREEN_LOADED) {
        screen->update();
    }
}

void screen_idle_timeout_reset(void)
{
    screen_manager.idle_timeout_ticks = 0;

    if (screen_manager.is_idle_timeout_triggered) {
        screen_set_current(SCREEN_EYES);
        screen_manager.is_idle_timeout_triggered = false;
    }
}

void screen_idle_timeout_trigger(void)
{
    screen_manager.is_idle_timeout_triggered = true;
}

static void screen_logic_process(void)
{
    /* ---- WAKEUP_ACK 超时退出 ---- */
    if (screen_manager.current_screen == SCREEN_WAKEUP_ACK &&
        app_ui_logic_get_state() != APP_EVT_STATE_ACTIVATION) {
        screen_manager.wakeup_ack_timeout_tick++;
        if (screen_manager.wakeup_ack_timeout_tick > screen_manager.wakeup_ack_timeout_target_tick) {
            screen_manager.wakeup_ack_timeout_tick = 0;
            if (screen_manager.old_screen == SCREEN_INFO ||
                screen_manager.old_screen == SCREEN_PAGE_SELECT) {
                screen_set_current(SCREEN_EYES);
            } else {
                screen_set_current(screen_manager.old_screen);
            }
        }
    }

    /* ---- 待机超时 → 切换到时钟页面并启动人脸检测 ---- */
    if (screen_manager.current_screen == SCREEN_EYES) {
        if (screen_manager.idle_timeout_ticks++ > APP_UI_MS_TO_TICKS(IDLE_TIMEOUT_MS)) {
            screen_manager.idle_timeout_ticks = 0;
            app_event_t evt = {.type = APP_EVT_STATE_IDLE};
            app_ui_logic_post(&evt, false);
        }
    }
}

static bool check_dual_long_press(void)
{
    uint32_t left_time  = screen_manager.key_state.left_long_press_time;
    uint32_t right_time = screen_manager.key_state.right_long_press_time;

    if (left_time == 0 || right_time == 0) {
        return false;
    }

    uint32_t time_diff = (left_time > right_time) ?
                         (left_time - right_time) :
                         (right_time - left_time);

    if (time_diff <= DUAL_LONG_PRESS_WINDOW_MS) {
        screen_manager.key_state.left_long_press_time  = 0;
        screen_manager.key_state.right_long_press_time = 0;
        return true;
    }

    return false;
}

void dooi_peripherals_init(screen_type_t screen_type)
{
    ;
}

void screen_ui_control(int key_id, touch_btn_event_t event)
{
    if (event == TOUCH_BTN_EVT_LONG_PRESS) {
        uint32_t now = get_tick_ms();

        if (key_id == TOUCH_BTN_KEY_LEFT) {
            screen_manager.key_state.left_long_press_time = now;
        } else if (key_id == TOUCH_BTN_KEY_RIGHT) {
            screen_manager.key_state.right_long_press_time = now;
        }

        if (check_dual_long_press()) {
            ESP_LOGI(TAG, "Dual long press detected, entering page select");
            screen_set_current(SCREEN_PAGE_SELECT);
            return;
        }
    }

    uint32_t now = get_tick_ms();
    if (screen_manager.key_state.left_long_press_time > 0 &&
        (now - screen_manager.key_state.left_long_press_time) > 2000) {
        screen_manager.key_state.left_long_press_time = 0;
    }
    if (screen_manager.key_state.right_long_press_time > 0 &&
        (now - screen_manager.key_state.right_long_press_time) > 2000) {
        screen_manager.key_state.right_long_press_time = 0;
    }

    screen_interface_t *current_interface = screen_interfaces[screen_manager.current_screen];
    if (current_interface && current_interface->on_key_event) {
        current_interface->on_key_event(key_id, event);
    }
}

void app_dooi_ui(void *arg)
{
    display_hal_init();
    lvgl_displays_init();

    screen_init();
    screen_set_current(SCREEN_WAKEUP_ACK);


    while (1) {
        screen_update();
        screen_logic_process();

        app_tip_led_run();
        vTaskDelay(pdMS_TO_TICKS(APP_UI_TICK_MS));
    }
}

void app_ui_init(void)
{
    xTaskCreatePinnedToCore(app_dooi_ui, "dooi_ui", 8192, (void *)1, 2, NULL, 1);
}

void ApplicationSendMessage(const char* message, bool use_tool){
    ;
}

void ApplicationPlaySound(SoundEffect_t sound){
    ;
}