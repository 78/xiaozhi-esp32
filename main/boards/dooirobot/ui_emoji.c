#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <lvgl.h>
#include "app_ui.h"

/* --- 颜色与基础尺寸定义 --- */
#define EMOJI_COLOR            lv_color_hex(0x00E5FF) // Nomi 科技青
#define PAGE_BG_COLOR          0x000000
#define EYE_DEFAULT_W          60
#define EYE_DEFAULT_H          100
#define EYE_RADIUS_MAX         100 // 调大以绝对保证胶囊感

typedef enum {
    EMOJI_NEUTRAL, EMOJI_HAPPY, EMOJI_LAUGHING, EMOJI_FUNNY, EMOJI_SAD,
    EMOJI_ANGRY, EMOJI_CRYING, EMOJI_LOVING, EMOJI_EMBARRASSED, EMOJI_SURPRISED,
    EMOJI_SHOCKED, EMOJI_THINKING, EMOJI_WINKING, EMOJI_COOL, EMOJI_RELAXED,
    EMOJI_DELICIOUS, EMOJI_KISSY, EMOJI_CONFIDENT, EMOJI_SLEEPY, EMOJI_SILLY,
    EMOJI_CONFUSED, EMOJI_MAX
} emoji_type_t;

typedef struct {
    int32_t w;       
    int32_t h;       
    int32_t angle;   // 旋转 (0.1度)
    int32_t x_ofs;   // X轴偏移
    int32_t y_ofs;   // Y轴偏移
} eye_state_t;

typedef struct {
    lv_obj_t *eye_obj;
    eye_state_t current_state;
} eye_widgets_t;

typedef struct {
    eye_widgets_t left;
    eye_widgets_t right;
    char current_emoji_name[32];
    emoji_type_t target_type;
    bool needs_refresh;
} emoji_ui_data_t;

static emoji_ui_data_t emoji_ctx = {0};
static SemaphoreHandle_t s_emoji_mutex = NULL;

static const char* emoji_names[] = {
    "neutral", "happy", "laughing", "funny", "sad", "angry", "crying", "loving",
    "embarrassed", "surprised", "shocked", "thinking", "winking", "cool", "relaxed",
    "delicious", "kissy", "confident", "sleepy", "silly", "confused"
};

/* --- 核心：带保护和动态轴心的动画回调 --- */
static void anim_w_cb(void *var, int32_t v) { 
    if (v < 6) v = 6; 
    lv_obj_set_width((lv_obj_t *)var, v); 
    lv_obj_set_style_transform_pivot_x((lv_obj_t *)var, v / 2, 0); 
}
static void anim_h_cb(void *var, int32_t v) { 
    if (v < 6) v = 6; 
    lv_obj_set_height((lv_obj_t *)var, v); 
    lv_obj_set_style_transform_pivot_y((lv_obj_t *)var, v / 2, 0); 
}
static void anim_angle_cb(void *var, int32_t v) { lv_obj_set_style_transform_rotation((lv_obj_t *)var, v, 0); }
static void anim_x_cb(void *var, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0); }
static void anim_y_cb(void *var, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0); }

/* --- Q弹动画引擎 --- */
static void anim_eye_to_state(eye_widgets_t *eye_data, eye_state_t target, uint32_t time_ms, uint32_t delay_ms) {
    if (!eye_data || !eye_data->eye_obj) return;
    lv_obj_t *obj = eye_data->eye_obj;
    eye_data->current_state = target;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot); // 开启过冲回弹曲线

    lv_anim_set_values(&a, lv_obj_get_width(obj), target.w);
    lv_anim_set_exec_cb(&a, anim_w_cb);
    lv_anim_start(&a);

    lv_anim_set_values(&a, lv_obj_get_height(obj), target.h);
    lv_anim_set_exec_cb(&a, anim_h_cb);
    lv_anim_start(&a);

    lv_anim_set_values(&a, lv_obj_get_style_transform_rotation(obj, 0), target.angle);
    lv_anim_set_exec_cb(&a, anim_angle_cb);
    lv_anim_start(&a);

    lv_anim_set_values(&a, lv_obj_get_style_translate_x(obj, 0), target.x_ofs);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_start(&a);

    lv_anim_set_values(&a, lv_obj_get_style_translate_y(obj, 0), target.y_ofs);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_start(&a);
}

/* --- 全量表情状态映射表 --- */
static void render_emoji_logic(emoji_type_t type) {
    eye_state_t l_state = {EYE_DEFAULT_W, EYE_DEFAULT_H, 0, 0, 0};
    eye_state_t r_state = {EYE_DEFAULT_W, EYE_DEFAULT_H, 0, 0, 0};
    uint32_t anim_time = 300; 

    switch(type) {
        /* -- 快乐系 -- */
        case EMOJI_HAPPY: 
        case EMOJI_LAUGHING: 
        case EMOJI_DELICIOUS:
            l_state = (eye_state_t){65, 50, 0, 0, -15};
            r_state = (eye_state_t){65, 50, 0, 0, -15};
            break;
            
        /* -- 愤怒系 (全新优化：极致凌厉的内八字) -- */
        case EMOJI_ANGRY:
            // 拉长(120)，变细(45)，倾斜角拉大到40度(400)，向中心狠狠挤压(25px)
            l_state = (eye_state_t){45, 120, 400, 25, 15};  // 左眼：顺时针40度 
            r_state = (eye_state_t){45, 120, -400, -25, 15}; // 右眼：逆时针40度 
            anim_time = 200; // 生气爆发要快
            break;

        /* -- 悲伤系 (全新优化：修长下坠的外八字) -- */
        case EMOJI_SAD:
        case EMOJI_CRYING:
            // 拉长(110)，变细(45)，外倾35度(350)，向外侧耷拉散开(-15px)
            l_state = (eye_state_t){45, 110, -350, -15, 25}; // 左眼：逆时针35度 
            r_state = (eye_state_t){45, 110, 350, 15, 25};  // 右眼：顺时针35度 
            anim_time = 450; 
            break;

        /* -- 惊讶系 -- */
        case EMOJI_SURPRISED:
            l_state = (eye_state_t){65, 120, 0, 0, -15};
            r_state = (eye_state_t){65, 120, 0, 0, -15};
            anim_time = 250;
            break;
        case EMOJI_SHOCKED:
            l_state = (eye_state_t){25, 25, 0, 0, 0};
            r_state = (eye_state_t){25, 25, 0, 0, 0};
            anim_time = 150; 
            break;

        /* -- 傲娇/耍酷系 -- */
        case EMOJI_COOL:
        case EMOJI_CONFIDENT:
            l_state = (eye_state_t){65, 55, 100, 0, 20};
            r_state = (eye_state_t){65, 55, 100, 0, 20};
            break;

        /* -- 滑稽/搞怪系 -- */
        case EMOJI_FUNNY:
        case EMOJI_SILLY:
        case EMOJI_CONFUSED:
            l_state = (eye_state_t){40, 40, -150, 0, -10};
            r_state = (eye_state_t){75, 75, 150, 0, 10};
            break;

        /* -- 互动系 -- */
        case EMOJI_WINKING:
        case EMOJI_KISSY:
            l_state = (eye_state_t){60, 12, 0, 0, 15};
            r_state = (eye_state_t){65, 105, 0, 0, -10};
            break;

        case EMOJI_EMBARRASSED:
            l_state = (eye_state_t){50, 75, 0, 35, 15};
            r_state = (eye_state_t){50, 75, 0, 35, 15};
            break;

        case EMOJI_THINKING:
            l_state = (eye_state_t){55, 65, 0, 25, -25};
            r_state = (eye_state_t){55, 65, 0, 25, -25};
            break;

        /* -- 放松系 -- */
        case EMOJI_SLEEPY:
        case EMOJI_RELAXED:
            l_state = (eye_state_t){65, 12, 0, 0, 25};
            r_state = (eye_state_t){65, 12, 0, 0, 25};
            anim_time = 600;
            break;

        /* -- 温暖系 -- */
        case EMOJI_LOVING:
            l_state = (eye_state_t){75, 90, 0, 0, 0};
            r_state = (eye_state_t){75, 90, 0, 0, 0};
            break;

        default: // EMOJI_NEUTRAL
            break;
    }

    // 右眼加入 30ms 的错位延迟
    anim_eye_to_state(&emoji_ctx.left, l_state, anim_time, 0);
    anim_eye_to_state(&emoji_ctx.right, r_state, anim_time, 30);
}

/* --- 外部 API --- */
void ui_emoji_set_expression(const char* name) {
    if (name == NULL || s_emoji_mutex == NULL) return;

    if (xSemaphoreTake(s_emoji_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(emoji_ctx.current_emoji_name, name, 31);
        for(int i=0; i<EMOJI_MAX; i++) {
            if(strcmp(name, emoji_names[i]) == 0) {
                emoji_ctx.target_type = (emoji_type_t)i;
                break;
            }
        }
        emoji_ctx.needs_refresh = true;
        xSemaphoreGive(s_emoji_mutex);
    }
}

/* --- 框架接口实现 --- */

static void setup_screen_bg(lv_obj_t *screen) {
    if (!screen) return;
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAGE_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_eye(lv_obj_t *parent, eye_widgets_t *eye) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 240, 240);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_center(cont);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    eye->eye_obj = lv_obj_create(cont);
    lv_obj_set_size(eye->eye_obj, EYE_DEFAULT_W, EYE_DEFAULT_H);
    lv_obj_set_style_radius(eye->eye_obj, EYE_RADIUS_MAX, 0);
    lv_obj_set_style_bg_color(eye->eye_obj, EMOJI_COLOR, 0);
    lv_obj_set_style_bg_opa(eye->eye_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye->eye_obj, 0, 0);
    
    // 初始化对齐和轴心
    lv_obj_align(eye->eye_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_transform_pivot_x(eye->eye_obj, EYE_DEFAULT_W/2, 0);
    lv_obj_set_style_transform_pivot_y(eye->eye_obj, EYE_DEFAULT_H/2, 0);

    eye->current_state = (eye_state_t){EYE_DEFAULT_W, EYE_DEFAULT_H, 0, 0, 0};
}

static int emoji_load(void) {
    static bool first_load = true;
    if(first_load) {
        first_load = false;
        if (s_emoji_mutex == NULL) s_emoji_mutex = xSemaphoreCreateMutex();
    }

    lv_obj_t *screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t *screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    if (!screen_left || !screen_right) return -1;

    setup_screen_bg(screen_left);
    setup_screen_bg(screen_right);

    create_eye(screen_left, &emoji_ctx.left);
    create_eye(screen_right, &emoji_ctx.right);

    render_emoji_logic(EMOJI_NEUTRAL);

    return 0;
}

static int emoji_update(void) {
    if (s_emoji_mutex == NULL) return 0;

    if (emoji_ctx.needs_refresh) {
        emoji_type_t type = EMOJI_NEUTRAL;
        if (xSemaphoreTake(s_emoji_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            type = emoji_ctx.target_type;
            emoji_ctx.needs_refresh = false;
            xSemaphoreGive(s_emoji_mutex);
        }
        render_emoji_logic(type);
    }

    /* 智能眨眼逻辑：避开不需要眨眼的表情 */
    static uint32_t next_blink = 0;
    bool can_blink = (emoji_ctx.target_type != EMOJI_SLEEPY && 
                      emoji_ctx.target_type != EMOJI_RELAXED &&
                      emoji_ctx.target_type != EMOJI_SHOCKED &&
                      emoji_ctx.target_type != EMOJI_WINKING);

    if(can_blink && lv_tick_get() > next_blink) {
        lv_anim_t a;
        lv_anim_init(&a);
        
        // 1. 极速闭眼
        lv_anim_set_time(&a, 100); 
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out); 
        
        lv_anim_set_var(&a, emoji_ctx.left.eye_obj);
        lv_anim_set_values(&a, emoji_ctx.left.current_state.h, 8);
        lv_anim_set_exec_cb(&a, anim_h_cb);
        lv_anim_start(&a);
        
        lv_anim_set_var(&a, emoji_ctx.right.eye_obj);
        lv_anim_set_values(&a, emoji_ctx.right.current_state.h, 8);
        lv_anim_start(&a);

        // 2. 带 Q弹 睁眼恢复
        lv_anim_set_time(&a, 250); 
        lv_anim_set_delay(&a, 80); 
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot); 
        
        lv_anim_set_var(&a, emoji_ctx.left.eye_obj);
        lv_anim_set_values(&a, 8, emoji_ctx.left.current_state.h);
        lv_anim_start(&a);

        lv_anim_set_var(&a, emoji_ctx.right.eye_obj);
        lv_anim_set_delay(&a, 100); 
        lv_anim_set_values(&a, 8, emoji_ctx.right.current_state.h);
        lv_anim_start(&a);

        next_blink = lv_tick_get() + 2500 + (rand() % 3500);
    }

    lv_timer_handler();
    return 0;
}

static int emoji_unload(void) {
    emoji_ctx.left.eye_obj = NULL;
    emoji_ctx.right.eye_obj = NULL;
    return 0;
}

static void emoji_on_key_event(int key_id, touch_btn_event_t event) {
    if (event == TOUCH_BTN_EVT_CLICK) {
        static int test_idx = 0;
        test_idx = (test_idx + 1) % EMOJI_MAX;
        ESP_LOGI("EMOJI", "Switch to: %s", emoji_names[test_idx]);
        ui_emoji_set_expression(emoji_names[test_idx]);
    }
}

screen_interface_t emoji_screen_interface = {
    .load = emoji_load,
    .update = emoji_update,
    .unload = emoji_unload,
    .on_key_event = emoji_on_key_event,
};