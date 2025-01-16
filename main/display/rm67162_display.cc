#include "rm67162_display.h"
#include "font_awesome_symbols.h"

#include "esp_lcd_sh8601.c"
#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <vector>

#define TAG "Rm67162Display"
#define LCD_LEDC_CH LEDC_CHANNEL_0

#define RM67162_LVGL_TICK_PERIOD_MS 2
#define RM67162_LVGL_TASK_MAX_DELAY_MS 20
#define RM67162_LVGL_TASK_MIN_DELAY_MS 1
#define RM67162_LVGL_TASK_STACK_SIZE (4 * 1024)
#define RM67162_LVGL_TASK_PRIORITY 10

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}
static void rm67162_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void rm67162_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    }
}
static void rm67162_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

void Rm67162Display::LvglTask()
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = RM67162_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (Lock())
        {
            task_delay_ms = lv_timer_handler();
            Unlock();
        }
        if (task_delay_ms > RM67162_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = RM67162_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < RM67162_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = RM67162_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {

    {0x11, (uint8_t[]){0x00}, 0, 120},
    // {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
    // {0x35, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, // 16bits-RGB565
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    // {0x51, (uint8_t[]){0xDF}, 1, 0},
};
Rm67162Display::Rm67162Display(esp_lcd_spi_bus_handle_t spi_bus, int cs, int rst,
                               int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : mirror_x_(mirror_x), mirror_y_(mirror_y), swap_xy_(swap_xy)
{
    static lv_disp_drv_t disp_drv;
    width_ = width;
    height_ = height;
    offset_x_ = offset_x;
    offset_y_ = offset_y;

    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");

    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(cs, notify_lvgl_flush_ready, &disp_drv);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(spi_bus, &io_config, &panel_io_));
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    // 初始化液晶屏驱动芯片
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_LOGI(TAG, "Install SH8601 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io_, &panel_config, &panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));

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
    disp_drv.flush_cb = rm67162_lvgl_flush_cb;
    disp_drv.drv_update_cb = rm67162_lvgl_port_update_callback;
    disp_drv.rounder_cb = rm67162_lvgl_rounder_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_;

    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void *arg)
        {
            lv_tick_inc(RM67162_LVGL_TICK_PERIOD_MS);
        },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LVGL Tick Timer",
        .skip_unhandled_events = false};
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer_, RM67162_LVGL_TICK_PERIOD_MS * 1000));
    lvgl_mutex_ = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mutex_ != nullptr);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate([](void *arg)
                {
        static_cast<Rm67162Display*>(arg)->LvglTask();
        vTaskDelete(NULL); }, "LVGL", RM67162_LVGL_TASK_STACK_SIZE, this, RM67162_LVGL_TASK_PRIORITY, NULL);

    SetupUI();
    InitBrightness();
    SetBacklight(brightness_);
}
void Rm67162Display::InitBrightness()
{
    Settings settings("display", false);
    brightness_ = settings.GetInt("bright", 80);
}
void Rm67162Display::SetBacklight(uint8_t brightness)
{
    brightness_ = brightness;
    if (brightness > 100)
    {
        brightness = 100;
    }
    Settings settings("display", true);
    settings.SetInt("bright", brightness_);

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
    // LEDC resolution set to 10bits, thus: 100% = 255
    uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
    int lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
}

void Rm67162Display::SetChatMessage(const std::string &role, const std::string &content)
{
    ESP_LOGI(TAG, "role: %s, content: %s", role.c_str(), content.c_str());
    // DisplayLockGuard lock(this);
    lv_obj_t *label = lv_label_create(content_);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    if (role == "user")
    {
        lv_obj_add_style(label, &style_user, 0);
    }
    else
    {
        lv_obj_add_style(label, &style_assistant, 0);
    }
    lv_obj_set_style_text_font(label, &font_puhui_14_1, 0);
    lv_label_set_text(label, content.c_str());
    lv_obj_center(label);

    lv_obj_set_style_pad_all(label, 5, LV_PART_MAIN);

    if (lv_obj_get_width(label) >= LV_HOR_RES)
        lv_obj_set_width(label, LV_HOR_RES);
    lv_obj_update_layout(label);
    lv_obj_scroll_to_view(label, LV_ANIM_ON);
}

Rm67162Display::~Rm67162Display()
{
    ESP_ERROR_CHECK(esp_timer_stop(lvgl_tick_timer_));
    ESP_ERROR_CHECK(esp_timer_delete(lvgl_tick_timer_));

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
    vSemaphoreDelete(lvgl_mutex_);
}

// void Rm67162Display::InitializeBacklight(gpio_num_t backlight_pin) {
//     if (backlight_pin == GPIO_NUM_NC) {
//         return;
//     }

//     // Setup LEDC peripheral for PWM backlight control
//     const ledc_channel_config_t backlight_channel = {
//         .gpio_num = backlight_pin,
//         .speed_mode = LEDC_LOW_SPEED_MODE,
//         .channel = LCD_LEDC_CH,
//         .intr_type = LEDC_INTR_DISABLE,
//         .timer_sel = LEDC_TIMER_0,
//         .duty = 0,
//         .hpoint = 0,
//         .flags = {
//             .output_invert = backlight_output_invert_,
//         }
//     };
//     const ledc_timer_config_t backlight_timer = {
//         .speed_mode = LEDC_LOW_SPEED_MODE,
//         .duty_resolution = LEDC_TIMER_10_BIT,
//         .timer_num = LEDC_TIMER_0,
//         .freq_hz = 5000,
//         .clk_cfg = LEDC_AUTO_CLK,
//         .deconfigure = false
//     };

//     ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
//     ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
// }

bool Rm67162Display::Lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to 0, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mutex_, timeout_ticks) == pdTRUE;
}

void Rm67162Display::Unlock()
{
    xSemaphoreGiveRecursive(lvgl_mutex_);
}

void Rm67162Display::SetupUI()
{
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(lv_disp_get_default());
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 18);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 2, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    /* Content */
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 1, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &font_awesome_14_1, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    emotion_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_14_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);

    battery_label_ = lv_label_create(status_bar_);
    
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);

    lv_style_init(&style_user);
    lv_style_set_radius(&style_user, 5);
    lv_style_set_bg_opa(&style_user, LV_OPA_COVER);
    lv_style_set_border_width(&style_user, 2);
    lv_style_set_border_color(&style_user, lv_color_hex(0));
    lv_style_set_pad_all(&style_user, 10);

    lv_style_set_text_color(&style_user, lv_color_hex(0));
    lv_style_set_bg_color(&style_user, lv_color_hex(0xE0E0E0));

    lv_style_init(&style_assistant);
    lv_style_set_radius(&style_assistant, 5);
    lv_style_set_bg_opa(&style_assistant, LV_OPA_COVER);
    lv_style_set_border_width(&style_assistant, 2);
    lv_style_set_border_color(&style_assistant, lv_color_hex(0));
    lv_style_set_pad_all(&style_assistant, 10);

    lv_style_set_text_color(&style_assistant, lv_color_hex(0xffffff));
    lv_style_set_bg_color(&style_assistant, lv_color_hex(0x00B050));
}
