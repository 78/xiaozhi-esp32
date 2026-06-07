#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <lvgl.h>
#include "app_ui.h"


#define MAX_NOTIFICATION_LEN 256
#define MAX_ACK_TOPIC_LEN 32

/* 字体定义 */
LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);

/* 统一风格 */
#define PAGE_BG_COLOR          0x000000
#define PANEL_BORDER_COLOR     0x3B82F6
#define TEXT_PRIMARY_COLOR     0xFFFFFF
#define TEXT_SECONDARY_COLOR   0xA0A0A0

static SemaphoreHandle_t s_wakeup_ack_mutex = NULL;

void dooi_enter_wifi_config_mode(void);

/* Wakeup ACK 屏幕私有数据 */
typedef struct {
    /* 左屏元素 */
    lv_obj_t *left_obj;           // 左屏圆形容器
    lv_obj_t *left_label;         // ACK主题文本

    /* 右屏元素 */
    lv_obj_t *right_obj;          // 右屏圆形容器
    lv_obj_t *right_label;        // 通知文本

    /* 数据缓存 */
    char ack_topic[MAX_ACK_TOPIC_LEN];
    char notification[MAX_NOTIFICATION_LEN];

    /* 状态标志 */
    bool needs_refresh;
} wakeup_ack_data_t;

static wakeup_ack_data_t wakeup_ack = {
    .left_obj = NULL,
    .left_label = NULL,
    .right_obj = NULL,
    .right_label = NULL,
    .ack_topic = "SYS MESSAGE",
    .notification = "正在初始化",
    .needs_refresh = false
};

/* 统一 240x240 根背景 */
static void setup_screen_bg(lv_obj_t *screen)
{
    if (!screen) return;

    /* 不 remove_style_all，保留主题基础能力 */
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAGE_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

/* 统一 200x200 圆形面板 */
static void setup_circle_panel(lv_obj_t *obj)
{
    if (!obj) return;

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 200, 200);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(PANEL_BORDER_COLOR), 0);
    lv_obj_set_style_clip_corner(obj, true, 0);
    lv_obj_set_style_pad_all(obj, 12, 0);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

/* 外部接口：设置ACK主题（用于左屏） */
void wakeup_ack_set_topic(const char* topic)
{
    if (topic == NULL || s_wakeup_ack_mutex == NULL) return;

    if (xSemaphoreTake(s_wakeup_ack_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(wakeup_ack.ack_topic, topic, MAX_ACK_TOPIC_LEN - 1);
        wakeup_ack.ack_topic[MAX_ACK_TOPIC_LEN - 1] = '\0';
        wakeup_ack.needs_refresh = true;
        xSemaphoreGive(s_wakeup_ack_mutex);
    }
}

/* 外部接口：添加通知消息（用于右屏） */
void wakeup_ack_add_notification(const char* notification)
{
    if (notification == NULL || s_wakeup_ack_mutex == NULL) return;

    if (xSemaphoreTake(s_wakeup_ack_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(wakeup_ack.notification, notification, MAX_NOTIFICATION_LEN - 1);
        wakeup_ack.notification[MAX_NOTIFICATION_LEN - 1] = '\0';
        wakeup_ack.needs_refresh = true;
        xSemaphoreGive(s_wakeup_ack_mutex);
    }
}

/* 创建左屏：圆形容器 + ACK主题 */
static void create_left_screen(lv_obj_t *parent)
{
    if (!parent) return;

    wakeup_ack.left_obj = lv_obj_create(parent);
    setup_circle_panel(wakeup_ack.left_obj);

    /* 左屏文本放到 panel 内，层级统一 */
    wakeup_ack.left_label = lv_label_create(wakeup_ack.left_obj);
    lv_label_set_text(wakeup_ack.left_label, wakeup_ack.ack_topic);
    lv_obj_set_style_text_color(wakeup_ack.left_label, lv_color_hex(TEXT_PRIMARY_COLOR), 0);
    lv_obj_set_style_text_font(wakeup_ack.left_label, &lv_font_notosans_cs_medium_14, 0);
    lv_obj_set_style_text_align(wakeup_ack.left_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(wakeup_ack.left_label, 150);
    lv_label_set_long_mode(wakeup_ack.left_label, LV_LABEL_LONG_WRAP);
    lv_obj_center(wakeup_ack.left_label);
}

/* 创建右屏：圆形容器 + 通知信息 */
static void create_right_screen(lv_obj_t *parent)
{
    if (!parent) return;

    wakeup_ack.right_obj = lv_obj_create(parent);
    setup_circle_panel(wakeup_ack.right_obj);

    wakeup_ack.right_label = lv_label_create(wakeup_ack.right_obj);
    lv_label_set_text(wakeup_ack.right_label, wakeup_ack.notification);
    lv_obj_set_style_text_color(wakeup_ack.right_label, lv_color_hex(TEXT_PRIMARY_COLOR), 0);
    lv_obj_set_style_text_font(wakeup_ack.right_label, &lv_font_notosans_cs_medium_14, 0);
    lv_obj_set_style_text_align(wakeup_ack.right_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(wakeup_ack.right_label, 150);
    lv_label_set_long_mode(wakeup_ack.right_label, LV_LABEL_LONG_WRAP);
    lv_obj_center(wakeup_ack.right_label);
}

/* 更新显示内容 */
static void update_display_content(void)
{
    if (s_wakeup_ack_mutex == NULL) return;

    /* 快速路径：无需刷新时不拿锁 */
    if (!wakeup_ack.needs_refresh) return;

    /* 拿锁，拷贝到局部缓冲区，立即释放锁 */
    char local_topic[MAX_ACK_TOPIC_LEN];
    char local_notification[MAX_NOTIFICATION_LEN];
    bool do_refresh = false;

    if (xSemaphoreTake(s_wakeup_ack_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (wakeup_ack.needs_refresh) {
            memcpy(local_topic, wakeup_ack.ack_topic, MAX_ACK_TOPIC_LEN);
            memcpy(local_notification, wakeup_ack.notification, MAX_NOTIFICATION_LEN);
            wakeup_ack.needs_refresh = false;
            do_refresh = true;
        }
        xSemaphoreGive(s_wakeup_ack_mutex);
    }

    /* 锁外操作 LVGL（LVGL 只在 UI 线程调用，无需二次加锁） */
    if (do_refresh) {
        if (wakeup_ack.left_label) {
            lv_label_set_text(wakeup_ack.left_label, local_topic);
        }
        if (wakeup_ack.right_label) {
            lv_label_set_text(wakeup_ack.right_label, local_notification);
        }
    }
}

/* 加载屏幕 */
static int wakeup_ack_load(void)
{
    static bool first_load = true;
    if(first_load)
    {
        first_load = false;
        if (s_wakeup_ack_mutex == NULL) {
            s_wakeup_ack_mutex = xSemaphoreCreateMutex();
        }
    }
    lv_obj_t *screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t *screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    if (!screen_left || !screen_right) {
        return -1;
    }

    /* 统一背景 */
    setup_screen_bg(screen_left);
    setup_screen_bg(screen_right);

    /* 创建左右屏内容 */
    create_left_screen(screen_left);
    create_right_screen(screen_right);

    return 0;
}

/* 更新屏幕 */
static int wakeup_ack_update(void)
{
    update_display_content();

    static uint16_t tick_counter = 0;
    if (tick_counter++ >= APP_UI_MS_TO_TICKS(500)) {
        tick_counter = 0;
        lv_timer_handler();
    }

    return 0;
}

/* 卸载屏幕 */
static int wakeup_ack_unload(void)
{
    if (wakeup_ack.left_obj) {
        lv_obj_del(wakeup_ack.left_obj);
        wakeup_ack.left_obj = NULL;
    }

    if (wakeup_ack.right_obj) {
        lv_obj_del(wakeup_ack.right_obj);
        wakeup_ack.right_obj = NULL;
    }

    /* 子对象会随父对象一起删，这里只清指针 */
    wakeup_ack.left_label = NULL;
    wakeup_ack.right_label = NULL;

    return 0;
}

static void wakeup_ack_on_key_event(int key_id, touch_btn_event_t event)
{
    (void)key_id;

    if(app_ui_logic_is_init_done() == false && app_ui_logic_get_state() == APP_EVT_STATE_CONNECT_WIFI)
    {
        dooi_enter_wifi_config_mode();
        return;
    }

    /* Wakeup ACK 页面通常自动消失，可以添加手动跳过功能 */
    if (event == TOUCH_BTN_EVT_CLICK || event == TOUCH_BTN_EVT_DOUBLE_CLICK) {
        if (screen_manager.old_screen == SCREEN_INFO ||
            screen_manager.old_screen == SCREEN_PAGE_SELECT) {
            screen_set_current(SCREEN_EYES);
        } else {
            screen_set_current(screen_manager.old_screen);
        }
    }
}

/* 屏幕接口 */
screen_interface_t wakeup_ack_screen_interface = {
    .load = wakeup_ack_load,
    .update = wakeup_ack_update,
    .unload = wakeup_ack_unload,
    .on_key_event = wakeup_ack_on_key_event,
};