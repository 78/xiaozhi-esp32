#include "lvgl.h"
#include "stdio.h"
#include "esp_random.h"

lv_obj_t *label_ask;
lv_obj_t *label_reply;
lv_obj_t *label_wifi_info;

// LV_IMG_DECLARE(img_bilibili120);
LV_FONT_DECLARE(font_alipuhui20);

static lv_timer_t *timer = NULL;

static const char *TAG = "ui_sr";

static bool g_sr_anim_active = false;
static int32_t g_sr_anim_count = 0;
static lv_obj_t *g_sr_label = NULL;
static lv_obj_t *g_online_sr_label = NULL;
static lv_obj_t *g_ask_sr_label = NULL;

static lv_obj_t *g_sr_mask = NULL;
static lv_obj_t *g_sr_bar[8] = {NULL};
#define LIMIT(x, min, max) ((x) <= (min) ? (min) : ((x) >= (max) ? (max) : (x)))
uint8_t biaoqing = 0;
// uint16_t Get_Set_Random(uint8_t set)
// {
// 	return LIMIT(esp_random() % set, 1, set);
// }
static int int16_sin(int32_t deg)
{
    static const int16_t sin_0_90_table[] = {
        0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126, 5690, 6252, 6813, 7371, 7927, 8481,
        9032, 9580, 10126, 10668, 11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886, 16383, 16876,
        17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621, 21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964,
        24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087, 28377, 28659, 28932, 29196,
        29451, 29697, 29934, 30162, 30381, 30591, 30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
        32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762, 32767};

    if (deg < 0)
    {
        deg = -deg;
    }

    deg = deg % 360;

    if (deg <= 90)
    {
        return sin_0_90_table[deg];
    }
    else if (deg <= 180)
    {
        return sin_0_90_table[180 - deg];
    }
    else if (deg <= 270)
    {
        return -sin_0_90_table[deg - 270];
    }
    else
    {
        return -sin_0_90_table[360 - deg];
    }
}
static void sr_mask_event_handler(lv_event_t *event)
{
    bool active = (bool)event->param;

    if (active)
    {
        lv_indev_t *indev = lv_indev_get_next(NULL);
        lv_indev_enable(indev, false);
        g_sr_anim_count = 0;
        g_sr_anim_active = true;
        lv_obj_clear_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_sr_mask);
    }
    else
    {
        lv_indev_t *indev = lv_indev_get_next(NULL);
        lv_indev_enable(indev, true);
        g_sr_anim_active = false;
    }
}

static void ui_speech_anim_cb(lv_timer_t *timer)
{
    const int32_t step = 40;

    if (g_sr_anim_active)
    {
        // /* Will hide hint message after wakeup */
        // static bool hint_hide = false;
        // if (hint_hide != true) {
        //     ui_hint(false);
        //     hint_hide = true;
        // }

        /* Show sr animate mask and move to foreground */
        if (lv_obj_has_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN))
        {
            lv_obj_clear_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(g_sr_mask);
        }

        /* Set bar value */
        int32_t sr_val[4] = {
            abs(int16_sin(g_sr_anim_count * step + step * 0)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 1)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 2)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 3)) / (32768 >> 7),
        };

        for (size_t i = 0; i < 4; i++)
        {
            lv_bar_set_value(g_sr_bar[i], sr_val[i] > 20 ? sr_val[i] : 20, LV_ANIM_ON);
            lv_bar_set_value(g_sr_bar[7 - i], sr_val[i] > 20 ? sr_val[i] : 20, LV_ANIM_ON);
            lv_bar_set_start_value(g_sr_bar[i], sr_val[i] > 20 ? -sr_val[i] : -20, LV_ANIM_ON);
            lv_bar_set_start_value(g_sr_bar[7 - i], sr_val[i] > 20 ? -sr_val[i] : -20, LV_ANIM_ON);
        }
        g_sr_anim_count++;
    }
    else
    {
        /* The first timer callback will set the bars to 0 */
        if (g_sr_anim_count != 0)
        {
            for (size_t i = 0; i < 8; i++)
            {
                lv_bar_set_value(g_sr_bar[i], 0, LV_ANIM_ON);
                lv_bar_set_start_value(g_sr_bar[i], -0, LV_ANIM_ON);
            }
            g_sr_anim_count = 0;
        }
        else
        {
            /* The second timer callback will hide sr mask */
            if (!lv_obj_has_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN))
            {
                lv_obj_add_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}
void sr_anim_start(void)
{
    // lv_textarea_set_text(g_online_sr_label, "");
    if (g_sr_mask != NULL)
    {
        lv_obj_move_foreground(g_sr_mask);
        lv_event_send(g_sr_mask, LV_EVENT_VALUE_CHANGED, (void *)true);
    }
}

void sr_anim_stop(void)
{
    if (g_sr_mask != NULL)
        lv_event_send(g_sr_mask, LV_EVENT_VALUE_CHANGED, (void *)false);
}
// 开机界面
void lv_gui_start(void)
{
    // 显示开机GIF图片
    // lv_obj_t *gif_start = lv_gif_create(lv_scr_act());
    // lv_gif_set_src(gif_start, &img_bilibili120);
    // lv_obj_align(gif_start, LV_ALIGN_CENTER, 0, -20);

    // 显示wifi连接信息
    label_wifi_info = lv_label_create(lv_scr_act());
    lv_obj_align(label_wifi_info, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_set_style_text_font(label_wifi_info, &font_alipuhui20, LV_STATE_DEFAULT);
    lv_label_set_text(label_wifi_info, "正在连接wifi...");
}
void label_ask_set_text(char *text)
{
    if (label_ask != NULL)
        lv_event_send(label_ask, LV_EVENT_VALUE_CHANGED, (void *)text);
}
void label_reply_set_text(char *text)
{
    if (label_reply != NULL)
        lv_event_send(label_reply, LV_EVENT_VALUE_CHANGED, (void *)text);
}
static void sr1_label_event_handler(lv_event_t *event)
{
    char *text = (char *)event->param;
    if (NULL != text)
    {
        printf("lv_label_set_text_static 1:%s\n", text);
        lv_label_set_text_static(label_ask, text);
    }
}
static void sr2_label_event_handler(lv_event_t *event)
{
    char *text = (char *)event->param;
    if (NULL != text)
    {
        printf("lv_label_set_text_static 2:%s\n", text);

        lv_label_set_text_static(label_reply, text);
    }
}

// 主界面
void lv_main_page(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); // 修改背景为黑色

    // static lv_style_t style;
    // lv_style_init(&style);
    // lv_style_set_radius(&style, 10); // 设置圆角半径
    // lv_style_set_bg_opa(&style, LV_OPA_COVER);
    // lv_style_set_bg_color(&style, lv_color_hex(0xFFFFFF));
    // lv_style_set_border_width(&style, 0);
    // lv_style_set_pad_all(&style, 10);
    // lv_style_set_width(&style, 280);  // 设置宽
    // lv_style_set_height(&style, 240); // 设置高

    // /*Create an object with the new style*/
    // lv_obj_t *obj = lv_obj_create(lv_scr_act());
    // lv_obj_add_style(obj, &style, 0);
    // ESP_LOGI(TAG, "sr animation initialize");

    label_ask = lv_label_create(lv_scr_act());
    lv_obj_set_width(label_ask, 260);
    lv_label_set_long_mode(label_ask, LV_LABEL_LONG_SCROLL_CIRCULAR); /*Circular scroll*/
    lv_obj_align(label_ask, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_text_font(label_ask, &font_alipuhui20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_ask, lv_palette_main(LV_PALETTE_GREEN), 0);

    lv_label_set_text(label_ask, "");
    lv_obj_add_event_cb(label_ask, sr1_label_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_move_foreground(label_ask);

    label_reply = lv_label_create(lv_scr_act());
    lv_obj_set_width(label_reply, 260);
    // lv_obj_set_height(label_reply, 200);
    // lv_label_set_long_mode(label_ask, LV_LABEL_LONG_SCROLL_CIRCULAR); /*Circular scroll*/
    lv_obj_align(label_reply, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_text_font(label_reply, &font_alipuhui20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_reply, lv_palette_main(LV_PALETTE_RED), 0);

    lv_label_set_text(label_reply, "AI:");
    lv_obj_add_event_cb(label_reply, sr2_label_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_move_foreground(label_reply);
    g_sr_mask = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(g_sr_mask, lv_obj_get_width(lv_obj_get_parent(g_sr_mask)), lv_obj_get_height(lv_obj_get_parent(g_sr_mask)));
    lv_obj_set_size(g_sr_mask, lv_obj_get_width(lv_obj_get_parent(g_sr_mask)), 100);
    lv_obj_clear_flag(g_sr_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_sr_mask, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(g_sr_mask, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_sr_mask, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_sr_mask, lv_obj_get_style_bg_color(lv_obj_get_parent(g_sr_mask), LV_PART_MAIN), LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(g_sr_mask, LV_OPA_TRANSP, 0);
    // lv_obj_align(g_sr_mask, LV_ALIGN_CENTER, 0, 0);

    lv_obj_align(g_sr_mask, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(g_sr_mask, sr_mask_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *obj_img = NULL;
    obj_img = lv_obj_create(g_sr_mask);
    lv_obj_set_size(obj_img, 80, 80);
    lv_obj_clear_flag(obj_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj_img, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj_img, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj_img, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(obj_img, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_align(obj_img, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *img_mic_logo = lv_img_create(obj_img);
    LV_IMG_DECLARE(img_src_mic_logo)
    lv_img_set_src(img_mic_logo, &img_src_mic_logo);
    lv_obj_center(img_mic_logo);

    for (size_t i = 0; i < sizeof(g_sr_bar) / sizeof(g_sr_bar[0]); i++)
    {
        g_sr_bar[i] = lv_bar_create(g_sr_mask);
        lv_obj_set_size(g_sr_bar[i], 5, 60);
        lv_obj_set_style_anim_time(g_sr_bar[i], 400, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_sr_bar[i], lv_color_make(237, 238, 239), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_sr_bar[i], lv_color_make(246, 175, 171), LV_PART_INDICATOR);
        lv_bar_set_range(g_sr_bar[i], -100, 100);
        lv_bar_set_value(g_sr_bar[i], 20, LV_ANIM_OFF);
        lv_bar_set_start_value(g_sr_bar[i], -20, LV_ANIM_OFF);
        lv_obj_set_style_anim_time(g_sr_bar[i], 400, LV_STATE_DEFAULT);
    }

    for (size_t i = 0; i < sizeof(g_sr_bar) / sizeof(g_sr_bar[0]) / 2; i++)
    {
        lv_obj_align_to(g_sr_bar[i], obj_img, LV_ALIGN_OUT_LEFT_MID, 15 * i - 65, 0);
        lv_obj_align_to(g_sr_bar[i + 4], obj_img, LV_ALIGN_OUT_RIGHT_MID, 15 * i + 20, 0);
    }

    g_sr_anim_count = 0;
    g_sr_anim_active = false;
    lv_timer_create(ui_speech_anim_cb, 200, NULL);
}