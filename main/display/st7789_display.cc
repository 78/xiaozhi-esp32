#include "st7789_display.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <driver/ledc.h>
#include <vector>

#define TAG "St7789Display"
#define LCD_LEDC_CH LEDC_CHANNEL_0

#define ST7789_LVGL_TICK_PERIOD_MS 2
#define ST7789_LVGL_TASK_MAX_DELAY_MS 20
#define ST7789_LVGL_TASK_MIN_DELAY_MS 1
#define ST7789_LVGL_TASK_STACK_SIZE (4 * 1024)
#define ST7789_LVGL_TASK_PRIORITY 10
LV_FONT_DECLARE(font_dingding);
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

static SemaphoreHandle_t lvgl_mux = NULL;
static lv_disp_drv_t disp_drv;
static void st7789_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(&disp_drv);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void st7789_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_ST7789_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_ST7789_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_ST7789_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
#if CONFIG_ST7789_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    }
}
static void st7789_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(ST7789_LVGL_TICK_PERIOD_MS);
}
static bool st7789_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void st7789_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void st7789_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = ST7789_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (st7789_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            st7789_lvgl_unlock();
        }
        if (task_delay_ms > ST7789_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = ST7789_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < ST7789_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = ST7789_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}


St7789Display::St7789Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           gpio_num_t backlight_pin, bool backlight_output_invert,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : panel_io_(panel_io), panel_(panel), backlight_pin_(backlight_pin), backlight_output_invert_(backlight_output_invert),
      mirror_x_(mirror_x), mirror_y_(mirror_y), swap_xy_(swap_xy)
{
    width_ = width;
    height_ = height;
    offset_x_ = offset_x;
    offset_y_ = offset_y;

    
    InitializeBacklight(backlight_pin);

    // draw white
    // std::vector<uint16_t> buffer(width_, 0xFFFF);
    // for (int y = 0; y < height_; y++)
    // {
    //     esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    // }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(width_ * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(width_ * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, width_ * 10);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width_;
    disp_drv.ver_res = height_;
    disp_drv.offset_x = offset_x_;
    disp_drv.offset_y = offset_y_;
    disp_drv.flush_cb = st7789_lvgl_flush_cb;
    disp_drv.drv_update_cb = st7789_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &st7789_increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, ST7789_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(st7789_lvgl_port_task, "LVGL", ST7789_LVGL_TASK_STACK_SIZE, NULL, ST7789_LVGL_TASK_PRIORITY, NULL);

    SetBacklight(100);

    SetupUI();
}

St7789Display::~St7789Display()
{
    if (content_ != nullptr)
    {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr)
    {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr)
    {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr)
    {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr)
    {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr)
    {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

void St7789Display::InitializeBacklight(gpio_num_t backlight_pin)
{
    if (backlight_pin == GPIO_NUM_NC)
    {
        return;
    }
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = backlight_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = backlight_output_invert_,
        }};
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false};

    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
}

void St7789Display::SetBacklight(uint8_t brightness)
{
    if (backlight_pin_ == GPIO_NUM_NC)
    {
        return;
    }

    if (brightness > 100)
    {
        brightness = 100;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
    // LEDC resolution set to 10bits, thus: 100% = 1023
    uint32_t duty_cycle = (1023 * brightness) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
}

void St7789Display::Lock() {
    // lvgl_port_lock(0);
    st7789_lvgl_lock(0);
}

void St7789Display::Unlock() {
    // lvgl_port_unlock();
    st7789_lvgl_unlock();
}

void St7789Display::SetupUI()
{
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(lv_disp_get_default());
    // auto screen = lv_scr_act();
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    // container_ = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(container_, 240, 280);
    // lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    // lv_obj_set_style_pad_all(container_, 0, 0);
    // lv_obj_set_style_border_width(container_, 0, 0);
    // lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    /* Status bar */
    status_bar_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_bar_, LV_HOR_RES - 40, 40);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    // lv_obj_set_x(status_bar_, 5);
    lv_obj_set_y(status_bar_, 20);
    lv_obj_set_align(status_bar_, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x000000), 0);

    /* Content */
    // content_ = lv_obj_create(lv_scr_act());
    // lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    // lv_obj_set_style_radius(content_, 0, 0);
    // lv_obj_set_width(content_, LV_HOR_RES);
    // lv_obj_set_flex_grow(content_, 1);

    emotion_label_ = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    // lv_obj_center(emotion_label_);
    lv_obj_set_style_text_color(emotion_label_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_align(emotion_label_, LV_ALIGN_CENTER, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &font_awesome_14_1, 0);
    lv_obj_set_style_text_color(network_label_, lv_palette_main(LV_PALETTE_GREEN), 0);

    // lv_obj_set_x(network_label_, 30);
    // lv_obj_set_y(network_label_, 30);
    // lv_obj_set_align(network_label_, LV_ALIGN_TOP_LEFT);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(notification_label_, &font_dingding, 0);
    lv_obj_set_style_text_color(notification_label_, lv_palette_main(LV_PALETTE_GREEN), 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, &font_dingding, 0);
    lv_obj_set_style_text_color(status_label_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);
    // lv_obj_set_x(battery_label_, 220);
    // lv_obj_set_y(battery_label_, 30);
    lv_obj_set_align(battery_label_, LV_ALIGN_TOP_RIGHT);

    reply_label_ = lv_label_create(lv_scr_act());
    lv_obj_set_width(reply_label_, LV_HOR_RES);
    lv_obj_set_height(reply_label_, 100);
    lv_obj_set_flex_grow(reply_label_, 2);
    // lv_label_set_long_mode(reply_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(reply_label_, "XiaoZhi AI");
    lv_obj_set_style_text_align(reply_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(reply_label_, &font_dingding, 0);
    lv_obj_set_style_text_color(reply_label_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_align(reply_label_, LV_ALIGN_BOTTOM_MID);
    // lv_obj_set_y(reply_label_, -50);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);


}
