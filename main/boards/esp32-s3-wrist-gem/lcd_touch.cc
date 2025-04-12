#include "config.h"
#include "application.h"
#include "lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_touch.h"


#if DISPLAY_TOUCH_AS_LISTEN_BUTTON
#include <esp_log.h>

#define TOUCH_PRESSED           LV_INDEV_STATE_PRESSED
#define TOUCH_RELEASED          LV_INDEV_STATE_RELEASED

static const char *TAG = "TOUCH";
static esp_lcd_touch_handle_t touch_handle;
static lv_indev_t *lvgl_touch_indev = NULL;
static int8_t lcd_touch_status;

void lvgl_touch_cb(lv_indev_t *indev_drv, lv_indev_data_t *data);

void SpiLcdDisplayEx::InitializeTouch(i2c_master_bus_handle_t i2c_bus) {
    
    lcd_touch_status = TOUCH_RELEASED;

    ESP_LOGD(TAG, "Install LCD touch driver");
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

    // Attach the TOUCH to the I2C bus
    if (esp_lcd_new_panel_io_i2c_v2(i2c_bus, &tp_io_config, &tp_io_handle) != ESP_OK) {
        ESP_LOGD(TAG, "Failed");
        return;
    }

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_WIDTH,
        .y_max = DISPLAY_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = DISPLAY_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_LOGI(TAG, "Initialize touch controller");
    if (esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &touch_handle) != ESP_OK) {
        ESP_LOGD(TAG, "Failed");
        return;
    }
    
    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = display_,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    lv_indev_set_read_cb(lvgl_touch_indev, lvgl_touch_cb);  // hook read

    // disable io log
    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);
    esp_log_level_set("CST816S", ESP_LOG_NONE);
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
}

void SimuOnPressDown() {
    if (lcd_touch_status == TOUCH_PRESSED) {
        return;
    }

    ESP_LOGI(TAG, "PressDown");

    lcd_touch_status = TOUCH_PRESSED;
}

void SimuOnPressUp() {
    if (lcd_touch_status == TOUCH_RELEASED) {
        return;
    }
    ESP_LOGI(TAG, "PressUp");
    auto& app = Application::GetInstance();
    app.ToggleChatState();
    // if (app.GetDeviceState() == kDeviceStateSpeaking) {
    //     app.AbortSpeaking(kAbortReasonNone);
    // }
    lcd_touch_status = TOUCH_RELEASED;
}

// take from 'esp_lvgl_port_touch.c'
typedef struct {
    esp_lcd_touch_handle_t  handle;     /* LCD touch IO handle */
    lv_indev_t              *indev;     /* LVGL input device driver */
} lvgl_port_touch_ctx_t;

void lvgl_touch_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    assert(indev_drv);
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)lv_indev_get_driver_data(indev_drv);
    assert(touch_ctx);
    assert(touch_ctx->handle);

    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read data from touch controller into memory */
    esp_lcd_touch_read_data(touch_ctx->handle);

    /* Read data from touch controller */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_ctx->handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        SimuOnPressDown();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        SimuOnPressUp();
    }
}

#else // DISPLAY_TOUCH_AS_LISTEN_BUTTON ==0
void InitializeTouch() {
}
#endif // DISPLAY_TOUCH_AS_LISTEN_BUTTON