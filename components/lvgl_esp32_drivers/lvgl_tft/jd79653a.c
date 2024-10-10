/**
@file jd79653a.c
@brief   GoodDisplay GDEW0154M09 e-paper display w/ FitiPower JD79653A
@version 1.0
@date    2020-08-28
@author  Jackson Ming Hu <huming2207@gmail.com>


@section LICENSE

MIT License

Copyright (c) 2020 Jackson Ming Hu

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "disp_spi.h"
#include "jd79653a.h"

#define TAG "lv_jd79653a"

#define PIN_DC              CONFIG_LV_DISP_PIN_DC
#define PIN_DC_BIT          ((1ULL << (uint8_t)(CONFIG_LV_DISP_PIN_DC)))
#define PIN_RST             CONFIG_LV_DISP_PIN_RST
#define PIN_RST_BIT         ((1ULL << (uint8_t)(CONFIG_LV_DISP_PIN_RST)))
#define PIN_BUSY            CONFIG_LV_DISP_PIN_BUSY
#define PIN_BUSY_BIT        ((1ULL << (uint8_t)(CONFIG_LV_DISP_PIN_BUSY)))
#define EVT_BUSY            (1UL << 0UL)
#define EPD_WIDTH           LV_HOR_RES_MAX
#define EPD_HEIGHT          LV_VER_RES_MAX
#define EPD_ROW_LEN         (EPD_HEIGHT / 8u)
#define EPD_PARTIAL_CNT     5;

#define BIT_SET(a, b)       ((a) |= (1U << (b)))
#define BIT_CLEAR(a, b)     ((a) &= ~(1U << (b)))

static uint8_t partial_counter = 0;

typedef struct
{
    uint8_t cmd;
    uint8_t data[3];
    size_t len;
} jd79653a_seq_t;

#define EPD_SEQ_LEN(x) ((sizeof(x) / sizeof(jd79653a_seq_t)))

static const uint8_t lut_vcom_dc1[] = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const uint8_t lut_ww1[] = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const uint8_t lut_bw1[] = {
    0x01, 0x84, 0x84, 0x83, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const uint8_t lut_wb1[] = {
    0x01, 0x44, 0x44, 0x43, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const uint8_t lut_bb1[] = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const jd79653a_seq_t init_seq[] = {
#if defined (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT_INVERTED)
        {0x00, {0xd3, 0x0e},       2},                 // Panel settings
#elif defined(CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
        {0x00, {0xdf, 0x0e}, 2},                 // Panel settings
#else
#error "Unsupported orientation - only portrait modes are supported for now"
#endif
        {0x4d, {0x55}, 1},                             // Undocumented secret from demo code
        {0xaa, {0x0f}, 1},                             // Undocumented secret from demo code
        {0xe9, {0x02}, 1},                             // Undocumented secret from demo code
        {0xb6, {0x11}, 1},                             // Undocumented secret from demo code
        {0xf3, {0x0a}, 1},                             // Undocumented secret from demo code
        {0x61, {0xc8, 0x00, 0xc8}, 3},   // Resolution settings
        {0x60, {0x00}, 1},                             // TCON
        {0x50, {0x97}, 1},                             // VCOM sequence
        {0xe3, {0x00}, 1},                             // Power saving settings
};

static const jd79653a_seq_t power_on_seq[] = {
        {0x50, {0x97}, 1}, // VCOM sequence
        {0x04, {},     0}, // Power on
};


static const jd79653a_seq_t power_off_seq[] = {
        {0x50, {0xf7}, 1}, // VCOM sequence
        {0x02, {},     0},      // Power off
};

static EventGroupHandle_t jd79653a_evts = NULL;

static void IRAM_ATTR jd79653a_busy_intr(void *arg)
{
    BaseType_t xResult;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xResult = xEventGroupSetBitsFromISR(jd79653a_evts, EVT_BUSY, &xHigherPriorityTaskWoken);
    if (xResult == pdPASS) {
        portYIELD_FROM_ISR();
    }
}

static void jd79653a_spi_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(PIN_DC, 0);     // DC = 0 for command
    disp_spi_send_data(&cmd, 1);
}

static void jd79653a_spi_send_data(uint8_t *data, size_t len)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(PIN_DC, 1);  // DC = 1 for data
    disp_spi_send_data(data, len);
}

static void jd79653a_spi_send_fb(uint8_t *data, size_t len)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(PIN_DC, 1);   // DC = 1 for data
    disp_spi_send_colors(data, len);
}

static void jd79653a_spi_send_seq(const jd79653a_seq_t *seq, size_t len)
{
    ESP_LOGD(TAG, "Writing cmd/data sequence, count %u", len);

    if (!seq || len < 1) return;
    for (size_t cmd_idx = 0; cmd_idx < len; cmd_idx++) {
        jd79653a_spi_send_cmd(seq[cmd_idx].cmd);
        if (seq[cmd_idx].len > 0) {
            jd79653a_spi_send_data((uint8_t *) seq[cmd_idx].data, seq[cmd_idx].len);
        }
    }
}

static esp_err_t jd79653a_wait_busy(uint32_t timeout_ms)
{
    uint32_t wait_ticks = (timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    EventBits_t bits = xEventGroupWaitBits(jd79653a_evts,
                                           EVT_BUSY, // Wait for busy bit
                                           pdTRUE, pdTRUE,       // Clear on exit, wait for all
                                           wait_ticks);         // Timeout

    return ((bits & EVT_BUSY) != 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void jd79653a_power_on()
{
    jd79653a_spi_send_seq(power_on_seq, EPD_SEQ_LEN(power_on_seq));
    vTaskDelay(pdMS_TO_TICKS(10));
    jd79653a_wait_busy(0);
}

static void jd79653a_power_off()
{
    jd79653a_spi_send_seq(power_off_seq, EPD_SEQ_LEN(power_off_seq));
    vTaskDelay(pdMS_TO_TICKS(10));
    jd79653a_wait_busy(0);
}

static void jd79653a_load_partial_lut()
{
    jd79653a_spi_send_cmd(0x20); // LUT VCOM register
    jd79653a_spi_send_data((uint8_t *)lut_vcom_dc1, sizeof(lut_vcom_dc1));

    jd79653a_spi_send_cmd(0x21); // LUT White-to-White
    jd79653a_spi_send_data((uint8_t *)lut_ww1, sizeof(lut_ww1));

    jd79653a_spi_send_cmd(0x22); // LUT Black-to-White
    jd79653a_spi_send_data((uint8_t *)lut_bw1, sizeof(lut_bw1));

    jd79653a_spi_send_cmd(0x23); // LUT White-to-Black
    jd79653a_spi_send_data((uint8_t *)lut_wb1,sizeof(lut_wb1));

    jd79653a_spi_send_cmd(0x24); // LUT Black-to-Black
    jd79653a_spi_send_data((uint8_t *)lut_bb1, sizeof(lut_bb1));
}

static void jd79653a_partial_in()
{
    ESP_LOGD(TAG, "Partial in!");

    // Panel setting: accept LUT from registers instead of OTP
#if defined (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT_INVERTED)
    uint8_t pst_use_reg_lut[] = { 0xf3, 0x0e };
#elif defined(CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
    uint8_t pst_use_reg_lut[] = { 0xff, 0x0e };
#else
#error "Unsupported orientation - only portrait modes are supported for now"
#endif
    jd79653a_spi_send_cmd(0x00);
    jd79653a_spi_send_data(pst_use_reg_lut, sizeof(pst_use_reg_lut));

    // WORKAROUND: need to ignore OLD framebuffer or otherwise partial refresh won't work
    uint8_t vcom = 0xb7;
    jd79653a_spi_send_cmd(0x50);
    jd79653a_spi_send_data(&vcom, 1);

    // Dump LUT in
    jd79653a_load_partial_lut();

    // Go partial!
    jd79653a_spi_send_cmd(0x91);
}

static void jd79653a_partial_out()
{
    ESP_LOGD(TAG, "Partial out!");

    // Panel setting: use LUT from OTP
#if defined (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT_INVERTED)
    uint8_t pst_use_otp_lut[] = { 0xd3, 0x0e };
#elif defined(CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
    uint8_t pst_use_otp_lut[] = { 0xdf, 0x0e };
#else
#error "Unsupported orientation - only portrait modes are supported for now"
#endif
    jd79653a_spi_send_cmd(0x00);
    jd79653a_spi_send_data(pst_use_otp_lut, sizeof(pst_use_otp_lut));

    // WORKAROUND: re-enable OLD framebuffer to get a better full refresh
    uint8_t vcom = 0x97;
    jd79653a_spi_send_cmd(0x50);
    jd79653a_spi_send_data(&vcom, 1);

    // Out from partial!
    jd79653a_spi_send_cmd(0x92);
}

static void jd79653a_update_partial(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t *data)
{
    jd79653a_power_on();
    jd79653a_partial_in();
    ESP_LOGD(TAG, "x1: 0x%x, x2: 0x%x, y1: 0x%x, y2: 0x%x", x1, x2, y1, y2);

    size_t len = ((x2 - x1 + 1) * (y2 - y1 + 1)) / 8;
    ESP_LOGD(TAG, "Writing PARTIAL LVGL fb with len: %u", len);

    // Set partial window
    uint8_t ptl_setting[7] = { x1, x2, 0, y1, 0, y2, 0x01 };
    jd79653a_spi_send_cmd(0x90);
    jd79653a_spi_send_data(ptl_setting, sizeof(ptl_setting));

    uint8_t *data_ptr = data;

    jd79653a_spi_send_cmd(0x13);
    for (size_t h_idx = 0; h_idx < EPD_HEIGHT; h_idx++) {
        jd79653a_spi_send_data(data_ptr, EPD_ROW_LEN);
        data_ptr += EPD_ROW_LEN;
        len -= EPD_ROW_LEN;
    }

    ESP_LOGD(TAG, "Partial wait start");

    jd79653a_spi_send_cmd(0x12);
    jd79653a_wait_busy(0);

    ESP_LOGD(TAG, "Partial updated");
    jd79653a_partial_out();
    jd79653a_power_off();
}

void jd79653a_fb_set_full_color(uint8_t color)
{
    jd79653a_power_on();
    uint8_t old_data[EPD_ROW_LEN];
    memset(old_data, ~(color), EPD_ROW_LEN);

    // Fill OLD data (maybe not necessary)
    jd79653a_spi_send_cmd(0x10);
    for (size_t idx = 0; idx < EPD_HEIGHT; idx++) {
        jd79653a_spi_send_data(old_data, EPD_ROW_LEN);
    }

    // Fill NEW data
    jd79653a_spi_send_cmd(0x13);
    for (size_t h_idx = 0; h_idx < EPD_HEIGHT; h_idx++) {
        for (size_t w_idx = 0; w_idx < EPD_ROW_LEN; w_idx++) {
            jd79653a_spi_send_data(&color, sizeof(color));
        }
    }

    jd79653a_spi_send_cmd(0x12); // Issue refresh command
    vTaskDelay(pdMS_TO_TICKS(100));
    jd79653a_wait_busy(0);

    jd79653a_power_off();
}

void jd79653a_fb_full_update(uint8_t *data, size_t len)
{
    jd79653a_power_on();
    ESP_LOGD(TAG, "Performing full update, len: %u", len);

    uint8_t *data_ptr = data;

    // Fill OLD data (maybe not necessary)
    uint8_t old_data[EPD_ROW_LEN] = { 0 };
    jd79653a_spi_send_cmd(0x10);
    for (size_t idx = 0; idx < EPD_HEIGHT; idx++) {
        jd79653a_spi_send_data(old_data, EPD_ROW_LEN);
    }

    // Fill NEW data
    jd79653a_spi_send_cmd(0x13);
    for (size_t h_idx = 0; h_idx < EPD_HEIGHT; h_idx++) {
        jd79653a_spi_send_data(data_ptr, EPD_ROW_LEN);
        data_ptr += EPD_ROW_LEN;
        len -= EPD_ROW_LEN;
    }

    ESP_LOGD(TAG, "Rest len: %u", len);

    jd79653a_spi_send_cmd(0x12); // Issue refresh command
    vTaskDelay(pdMS_TO_TICKS(100));
    jd79653a_wait_busy(0);

    jd79653a_power_off();
}

void jd79653a_lv_set_fb_cb(struct _disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                           lv_color_t color, lv_opa_t opa)
{
    uint16_t byte_index = (x >> 3u) + (y * EPD_ROW_LEN);
    uint8_t bit_index = x & 0x07u;

    if (color.full) {
        BIT_SET(buf[byte_index], 7 - bit_index);
    } else {
        BIT_CLEAR(buf[byte_index], 7 - bit_index);
    }
}

void jd79653a_lv_rounder_cb(struct _disp_drv_t *disp_drv, lv_area_t *area)
{
    // Always send full framebuffer if it's not in partial mode
    area->x1 = 0;
    area->y1 = 0;
    area->x2 = EPD_WIDTH - 1;
    area->y2 = EPD_HEIGHT - 1;
}

void jd79653a_lv_fb_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    size_t len = ((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)) / 8;

    ESP_LOGD(TAG, "x1: 0x%x, x2: 0x%x, y1: 0x%x, y2: 0x%x", area->x1, area->x2, area->y1, area->y2);
    ESP_LOGD(TAG, "Writing LVGL fb with len: %u, partial counter: %u", len, partial_counter);

    uint8_t *buf = (uint8_t *) color_map;

    if (partial_counter == 0) {
        ESP_LOGD(TAG, "Refreshing in FULL");
        jd79653a_fb_full_update(buf, ((EPD_HEIGHT * EPD_WIDTH) / 8));
        partial_counter = EPD_PARTIAL_CNT; // Reset partial counter here
    } else {
        jd79653a_update_partial(area->x1, area->y1, area->x2, area->y2, buf);
        partial_counter -= 1;   // ...or otherwise, decrease 1
    }

    lv_disp_flush_ready(drv);
}

void jd79653a_deep_sleep()
{
    jd79653a_spi_send_seq(power_off_seq, EPD_SEQ_LEN(power_off_seq));
    jd79653a_wait_busy(1000);

    uint8_t check_code = 0xa5;
    jd79653a_spi_send_cmd(0x07);
    jd79653a_spi_send_data(&check_code, sizeof(check_code));
}

void jd79653a_init()
{
    // Initialise event group
    jd79653a_evts = xEventGroupCreate();
    if (!jd79653a_evts) {
        ESP_LOGE(TAG, "Failed when initialising event group!");
        return;
    }

    // Setup output pins, output (PP)
    gpio_config_t out_io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = PIN_DC_BIT | PIN_RST_BIT,
            .pull_down_en = 0,
            .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&out_io_conf));

    // Setup input pin, pull-up, input
    gpio_config_t in_io_conf = {
            .intr_type = GPIO_INTR_POSEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = PIN_BUSY_BIT,
            .pull_down_en = 0,
            .pull_up_en = 1,
    };
    ESP_ERROR_CHECK(gpio_config(&in_io_conf));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUSY, jd79653a_busy_intr, (void *) PIN_BUSY);

    // Hardware reset
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(15)); // At least 10ms, leave 15ms for now just in case...
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Dump in initialise sequence
    jd79653a_spi_send_seq(init_seq, EPD_SEQ_LEN(init_seq));
    ESP_LOGI(TAG, "Panel init sequence sent");

    // Check BUSY status here
    jd79653a_wait_busy(0);

    ESP_LOGI(TAG, "Panel is up!");
}
