#include "atk_st7789_80i.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <esp_err.h>
#include <vector>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"


#define TAG "atk_st7789"

#define  ATK_ST7789_80_LVGL_TICK_PERIOD_MS      2
#define  ATK_ST7789_80_LVGL_TASK_MAX_DELAY_MS   20
#define  ATK_ST7789_80_LVGL_TASK_MIN_DELAY_MS   1
#define  ATK_ST7789_80_LVGL_TASK_STACK_SIZE     (10 * 1024)
#define  ATK_ST7789_80_LVGL_TASK_PRIORITY       10

// Pin Definitions 
#define LCD_NUM_CS      GPIO_NUM_1
#define LCD_NUM_DC      GPIO_NUM_2
#define LCD_NUM_RD      GPIO_NUM_41
#define LCD_NUM_WR      GPIO_NUM_42
#define LCD_NUM_RST     GPIO_NUM_NC

#define GPIO_LCD_D0     GPIO_NUM_40
#define GPIO_LCD_D1     GPIO_NUM_39
#define GPIO_LCD_D2     GPIO_NUM_38
#define GPIO_LCD_D3     GPIO_NUM_12
#define GPIO_LCD_D4     GPIO_NUM_11
#define GPIO_LCD_D5     GPIO_NUM_10
#define GPIO_LCD_D6     GPIO_NUM_9
#define GPIO_LCD_D7     GPIO_NUM_46

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

static lv_disp_drv_t disp_drv;
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/**
* @brief    将内部缓冲区的内容刷新到显示屏上的特定区域
* @note     可以使用 DMA 或者任何硬件在后台加速执行这个操作
*           但是，需要在刷新完成后调用函数 'lv_disp_flush_ready()'
* @param    disp_drv : 显示设备
* @param    area : 要刷新的区域，包含了填充矩形的对角坐标
* @param    color_map : 颜色数组
* @retval   无
*/
static void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    /* 特定区域打点 */
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
}

void ATK_ST7789_80_Display::LvglTask()
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = ATK_ST7789_80_LVGL_TASK_MAX_DELAY_MS;

    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (Lock())
        {
            task_delay_ms = lv_timer_handler();
            Unlock();
        }
        if (task_delay_ms > ATK_ST7789_80_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = ATK_ST7789_80_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < ATK_ST7789_80_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = ATK_ST7789_80_LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

extern "C" void emoji_font_init();


ATK_ST7789_80_Display::ATK_ST7789_80_Display(gpio_num_t backlight_pin,
                                            int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
                                            :  backlight_pin_(backlight_pin),mirror_x_(mirror_x), mirror_y_(mirror_y), swap_xy_(swap_xy)
{
    width_ = 320;
    height_ = 240;

    width_ = width;
    height_ = height;
    offset_x_ = offset_x;
    offset_y_ = offset_y;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    static lv_disp_draw_buf_t disp_buf;         // contains internal graphic buffer(s) called draw buffer(s)
    gpio_config_t gpio_init_struct;
    InitializeBacklight(backlight_pin);         // light set
    emoji_font_init();

    /* 配置RD引脚 */
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;                 /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;                 /* 配置输出模式 */
    gpio_init_struct.pin_bit_mask = 1ull << LCD_NUM_RD;             /* 配置引脚位掩码 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;          /* 失能下拉 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;               /* 使能下拉 */
    gpio_config(&gpio_init_struct);                                 /* 引脚配置 */
    gpio_set_level(LCD_NUM_RD, 1);                                  /* RD管脚拉高 */

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {                         /* 初始化80并口总线 */
        .dc_gpio_num = LCD_NUM_DC,
        .wr_gpio_num = LCD_NUM_WR,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            GPIO_LCD_D0,
            GPIO_LCD_D1,
            GPIO_LCD_D2,
            GPIO_LCD_D3,
            GPIO_LCD_D4,
            GPIO_LCD_D5,
            GPIO_LCD_D6,
            GPIO_LCD_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = width_ * height_ * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };

    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));    /* 新建80并口总线 */

    esp_lcd_panel_io_i80_config_t io_config = {                     /* 80并口配置 */
        .cs_gpio_num = LCD_NUM_CS,
        .pclk_hz = (10 * 1000 * 1000),
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);                                  /* 复位屏幕 */
    esp_lcd_panel_init(panel_handle);                                   /* 初始化屏幕 */
    esp_lcd_panel_invert_color(panel_handle, true);                     /* 开启颜色反转 */
    esp_lcd_panel_set_gap(panel_handle, 0, 0);                          /* 设置XY偏移 */
    uint8_t data0[] = {0x00};
    uint8_t data1[] = {0x65};
    esp_lcd_panel_io_tx_param(io_handle, 0x36, data0, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0x3A, data1, 1);
    esp_lcd_panel_swap_xy(panel_handle, swap_xy);                       /* 不需要交换X和Y轴 */
    esp_lcd_panel_mirror(panel_handle, mirror_x, mirror_y);             /* 对屏幕的XY轴不进行镜像处理 */ 
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));     /* 启动屏幕 */

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    void *buf1 = NULL;
    void *buf2 = NULL;

    buf1 = heap_caps_malloc(width_ * 60 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(width_ * 60 * sizeof(lv_color_t), MALLOC_CAP_DMA);

    /* 初始化显示缓冲区 */
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, width_ * 60);     /* 初始化显示缓冲区 */

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width_;
    disp_drv.ver_res = height_;
    disp_drv.offset_x = offset_x_;
    disp_drv.offset_y = offset_y_;
    disp_drv.flush_cb = lvgl_disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void* arg) {
            lv_tick_inc(ATK_ST7789_80_LVGL_TICK_PERIOD_MS);
        },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LVGL Tick Timer",
        .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer_, ATK_ST7789_80_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mutex_ = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mutex_ != nullptr);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate([](void *arg) {
        static_cast<ATK_ST7789_80_Display*>(arg)->LvglTask();
        vTaskDelete(NULL);
    }, "LVGL", ATK_ST7789_80_LVGL_TASK_STACK_SIZE, this, ATK_ST7789_80_LVGL_TASK_PRIORITY, NULL);

    SetupUI();
}

ATK_ST7789_80_Display::~ATK_ST7789_80_Display()
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

void ATK_ST7789_80_Display::InitializeBacklight(gpio_num_t backlight_pin)
{
    if (backlight_pin == GPIO_NUM_NC)
    {
        return;
    }
 
    /* Setup LEDC peripheral for PWM backlight control */
}

bool ATK_ST7789_80_Display::Lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to 0, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mutex_, timeout_ticks) == pdTRUE;
}

void ATK_ST7789_80_Display::Unlock()
{
    xSemaphoreGiveRecursive(lvgl_mutex_);
}

void ATK_ST7789_80_Display::SetupUI()
{
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(lv_disp_get_default());
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

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
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    // lv_obj_center(emotion_label_);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.8); // 限制宽度为屏幕宽度的 80%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

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
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);
}

void ATK_ST7789_80_Display::SetChatMessage(const std::string &role, const std::string &content)
{
    if (chat_message_label_ == nullptr)
    {
        return;
    }

    lv_label_set_text(chat_message_label_, content.c_str());
}
