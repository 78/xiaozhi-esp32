#include "custom_lcd_display.h"

#include "lcd_display.h"

#include <algorithm>
#include <vector>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"
#include <cstring>
#include "settings.h"

#include "esp_lcd_panel_io.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "config.h"

#include "board.h"

#define TAG "CustomLcdDisplay"

static SemaphoreHandle_t trans_done_sem = NULL;
static uint16_t *trans_buf_1;



bool CustomLcdDisplay::lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    BaseType_t taskAwake = pdFALSE;
    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    if (trans_done_sem) {
        xSemaphoreGiveFromISR(trans_done_sem, &taskAwake);
    }
    return false;
}

void CustomLcdDisplay::lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map) {
    assert(drv != NULL);
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(drv);
    assert(panel_handle != NULL);
#if (DISPLAY_ROTATION_90 == true)
    // Full-frame render (LV_DISPLAY_RENDER_MODE_FULL) in landscape: rotate and byte-swap
    // straight into the internal DMA buffer, one band of panel rows at a time. This avoids
    // a full-frame swap plus rotation through a second PSRAM buffer on every frame.
    // LV_DISPLAY_ROTATION_90 maps panel pixel (px, py) to logical (x = HEIGHT - 1 - py, y = px).
    const uint16_t *src = (const uint16_t *)color_map;
    const int logical_w = DISPLAY_HEIGHT;
    const int band_rows = LVGL_DMA_BUFF_LEN / 2 / DISPLAY_WIDTH;
    xSemaphoreGive(trans_done_sem);
    for (int py0 = 0; py0 < DISPLAY_HEIGHT; py0 += band_rows) {
        int rows = std::min(band_rows, DISPLAY_HEIGHT - py0);
        xSemaphoreTake(trans_done_sem, portMAX_DELAY);
        // Read each logical row sequentially (cache friendly in PSRAM) and scatter into the
        // band in internal RAM. Band row r is logical column x = HEIGHT - 1 - (py0 + r).
        const int x_last = logical_w - 1 - py0;  // logical x for band row 0
        for (int y = 0; y < DISPLAY_WIDTH; y++) {
            const uint16_t *row = src + y * logical_w + (x_last - rows + 1);
            uint16_t *dst = trans_buf_1 + (rows - 1) * DISPLAY_WIDTH + y;
            for (int i = 0; i < rows; i++) {
                *dst = __builtin_bswap16(row[i]);
                dst -= DISPLAY_WIDTH;
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, py0, DISPLAY_WIDTH, py0 + rows, trans_buf_1);
    }
    xSemaphoreTake(trans_done_sem, portMAX_DELAY);
#else
    lv_draw_sw_rgb565_swap(color_map, lv_area_get_width(area) * lv_area_get_height(area));
    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap = (DISPLAY_HEIGHT / flush_coun);
    const int dmalen = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0;
    int offsety1 = 0;
    int offsetx2 = DISPLAY_WIDTH;
    int offsety2 = offgap;
    uint16_t *map = (uint16_t*)color_map;
    xSemaphoreGive(trans_done_sem);

    for(int i = 0; i<flush_coun; i++) {
        xSemaphoreTake(trans_done_sem,portMAX_DELAY);
        memcpy(trans_buf_1,map,LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(trans_done_sem,portMAX_DELAY);
#endif
    lv_disp_flush_ready(drv);
}


CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    trans_done_sem = xSemaphoreCreateBinary();
    trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);

    uint32_t buffer_size = 0;
    lv_color_t *buf1 = NULL;
    lvgl_port_lock(0);
    uint8_t color_bytes = lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    display_ = lv_display_create(width_, height_);
    lv_display_set_flush_cb(display_, lvgl_port_flush_callback);
    buffer_size = width_ * height_;
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(1, buffer_size * color_bytes, MALLOC_CAP_SPIRAM);
#if (DISPLAY_ROTATION_90 == true)
    lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_90);
#endif
    lv_display_set_buffers(display_, buf1, NULL, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(display_, panel_);
    lvgl_port_unlock();

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = lvgl_port_flush_io_ready_callback,
    };
    /* Register done callback */
    esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, display_);

    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}