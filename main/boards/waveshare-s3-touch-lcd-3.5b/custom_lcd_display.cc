
#include "config.h"
#include "custom_lcd_display.h"
#include "lcd_display.h"
#include "assets/lang_config.h"
#include "settings.h"
#include "board.h"

#include <vector>
#include <cstring>

#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>

#define TAG "CustomLcdDisplay"


static SemaphoreHandle_t trans_done_sem = NULL;
static uint16_t *trans_act;
static uint16_t *trans_buf_1;
static uint16_t *trans_buf_2;

bool CustomLcdDisplay::lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t taskAwake = pdFALSE;
    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    if (trans_done_sem) {
        xSemaphoreGiveFromISR(trans_done_sem, &taskAwake);
    }
    return false;
}

void CustomLcdDisplay::lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    assert(drv != NULL);
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_driver_data(drv);
    assert(panel_handle != NULL);

    size_t len = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(color_map, len);

    const int x_start = area->x1;
    const int x_end = area->x2;
    const int y_start = area->y1;
    const int y_end = area->y2;
    const int width = x_end - x_start + 1;
    const int height = y_end - y_start + 1;
    
    int32_t hor_res = lv_display_get_horizontal_resolution(drv);
    int32_t ver_res = lv_display_get_vertical_resolution(drv);

    // printf("hor_res: %ld, ver_res: %ld\r\n", hor_res, ver_res);
    // printf("x_start: %d, x_end: %d, y_start: %d, y_end: %d, width: %d, height: %d\r\n", x_start, x_end, y_start, y_end, width, height);
    uint16_t *from = (uint16_t *)color_map;
    uint16_t *to = NULL;

    if (DISPLAY_TRANS_SIZE > 0) {
        assert(trans_buf_1 != NULL);

        int x_draw_start = 0;
        int x_draw_end = 0;
        int y_draw_start = 0;
        int y_draw_end = 0;
        int trans_count = 0;

        trans_act = trans_buf_1;
        lv_display_rotation_t rotate = LV_DISPLAY_ROTATION;

        int x_start_tmp = 0;
        int x_end_tmp = 0;
        int max_width = 0;
        int trans_width = 0;

        int y_start_tmp = 0;
        int y_end_tmp = 0;
        int max_height = 0;
        int trans_height = 0;

        if (LV_DISPLAY_ROTATION_270 == rotate || LV_DISPLAY_ROTATION_90 == rotate) {
            max_width = ((DISPLAY_TRANS_SIZE / height) > width) ? (width) : (DISPLAY_TRANS_SIZE / height);
            trans_count = width / max_width + (width % max_width ? (1) : (0));

            x_start_tmp = x_start;
            x_end_tmp = x_end;
        } else {
            max_height = ((DISPLAY_TRANS_SIZE / width) > height) ? (height) : (DISPLAY_TRANS_SIZE / width);
            trans_count = height / max_height + (height % max_height ? (1) : (0));

            y_start_tmp = y_start;
            y_end_tmp = y_end;
        }

        for (int i = 0; i < trans_count; i++) {

            if (LV_DISPLAY_ROTATION_90 == rotate) {
                trans_width = (x_end - x_start_tmp + 1) > max_width ? max_width : (x_end - x_start_tmp + 1);
                x_end_tmp = (x_end - x_start_tmp + 1) > max_width ? (x_start_tmp + max_width - 1) : x_end;
            } else if (LV_DISPLAY_ROTATION_270 == rotate) {
                trans_width = (x_end_tmp - x_start + 1) > max_width ? max_width : (x_end_tmp - x_start + 1);
                x_start_tmp = (x_end_tmp - x_start + 1) > max_width ? (x_end_tmp - trans_width + 1) : x_start;
            } else if (LV_DISPLAY_ROTATION_0 == rotate) {
                trans_height = (y_end - y_start_tmp + 1) > max_height ? max_height : (y_end - y_start_tmp + 1);
                y_end_tmp = (y_end - y_start_tmp + 1) > max_height ? (y_start_tmp + max_height - 1) : y_end;
            } else {
                trans_height = (y_end_tmp - y_start + 1) > max_height ? max_height : (y_end_tmp - y_start + 1);
                y_start_tmp = (y_end_tmp - y_start + 1) > max_height ? (y_end_tmp - max_height + 1) : y_start;
            }

            trans_act = (trans_act == trans_buf_1) ? (trans_buf_2) : (trans_buf_1);
            to = trans_act;

            switch (rotate) {
            case LV_DISPLAY_ROTATION_90:
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < trans_width; x++) {
                        *(to + x * height + (height - y - 1)) = *(from + y * width + x_start_tmp + x);
                    }
                }
                x_draw_start = ver_res - y_end - 1;
                x_draw_end = ver_res - y_start - 1;
                y_draw_start = x_start_tmp;
                y_draw_end = x_end_tmp;
                break;
            case LV_DISPLAY_ROTATION_270:
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < trans_width; x++) {
                        *(to + (trans_width - x - 1) * height + y) = *(from + y * width + x_start_tmp + x);
                    }
                }
                x_draw_start = y_start;
                x_draw_end = y_end;
                y_draw_start = hor_res - x_end_tmp - 1;
                y_draw_end = hor_res - x_start_tmp - 1;
                break;
            case LV_DISPLAY_ROTATION_180:
                for (int y = 0; y < trans_height; y++) {
                    for (int x = 0; x < width; x++) {
                        *(to + (trans_height - y - 1)*width + (width - x - 1)) = *(from + y_start_tmp * width + y * (width) + x);
                    }
                }
                x_draw_start = hor_res - x_end - 1;
                x_draw_end = hor_res - x_start - 1;
                y_draw_start = ver_res - y_end_tmp - 1;
                y_draw_end = ver_res - y_start_tmp - 1;
                break;
            case LV_DISPLAY_ROTATION_0:
                for (int y = 0; y < trans_height; y++) {
                    for (int x = 0; x < width; x++) {
                        *(to + y * (width) + x) = *(from + y_start_tmp * width + y * (width) + x);
                    }
                }
                x_draw_start = x_start;
                x_draw_end = x_end;
                y_draw_start = y_start_tmp;
                y_draw_end = y_end_tmp;
                break;
            default:
                break;
            }

            if (0 == i) {
                // if (disp_ctx->draw_wait_cb) {
                //     disp_ctx->draw_wait_cb(disp_ctx->panel_handle->user_data);
                // }
                xSemaphoreGive(trans_done_sem);
            }

            xSemaphoreTake(trans_done_sem, portMAX_DELAY);
            // printf("i: %d, x_draw_start: %d, x_draw_end: %d, y_draw_start: %d, y_draw_end: %d\r\n", i, x_draw_start, x_draw_end, y_draw_start, y_draw_end);
            esp_lcd_panel_draw_bitmap(panel_handle, x_draw_start, y_draw_start, x_draw_end + 1, y_draw_end + 1, to);

            if (LV_DISPLAY_ROTATION_90 == rotate) {
                x_start_tmp += max_width;
            } else if (LV_DISPLAY_ROTATION_270 == rotate) {
                x_end_tmp -= max_width;
            } if (LV_DISPLAY_ROTATION_0 == rotate) {
                y_start_tmp += max_height;
            } else {
                y_end_tmp -= max_height;
            }
        }
    } else {
        esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end + 1, y_end + 1, color_map);
    }
    lv_disp_flush_ready(drv);
}

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {
    //     width_ = width;
    // height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    trans_done_sem = xSemaphoreCreateCounting(1, 0);
    trans_buf_1 = (uint16_t *)heap_caps_malloc(DISPLAY_TRANS_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    trans_buf_2 = (uint16_t *)heap_caps_malloc(DISPLAY_TRANS_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
#if 0
    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 0,
            .buff_spiram = 1,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 1,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    lv_display_set_flush_cb(display_, lvgl_port_flush_callback);
#else

    uint32_t buffer_size = 0;
    lv_color_t *buf1 = NULL;
    lvgl_port_lock(0);
    uint8_t color_bytes = lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    display_ = lv_display_create(width_, height_);
    lv_display_set_flush_cb(display_, lvgl_port_flush_callback);
    buffer_size = width_ * height_;
    buf1 = (lv_color_t *)heap_caps_aligned_alloc(1, buffer_size * color_bytes, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(display_, buf1, NULL, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_driver_data(display_, panel_);
    lvgl_port_unlock();

#endif

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = lvgl_port_flush_io_ready_callback,
    };
    /* Register done callback */
    esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, display_);

    esp_lcd_panel_disp_on_off(panel_, false);

    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }


    SetupUI();
}