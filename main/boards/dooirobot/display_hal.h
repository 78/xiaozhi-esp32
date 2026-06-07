/* ============== display_hal.h ============== */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "lvgl.h"

#define LCD_H_RES           240
#define LCD_V_RES           240
#define CHUNK_LINES         30                                /* 每块高度，240/30=8块 */
#define CHUNK_PIXELS        (LCD_H_RES * CHUNK_LINES)         /* 14400 像素 */
#define CHUNK_BUF_BYTES     (CHUNK_PIXELS * sizeof(uint16_t)) /* 28800 字节 */
#define NUM_DISPLAYS        2

#define LCD1_HOST          SPI2_HOST   // HSPI
#define LCD2_HOST          SPI3_HOST   // VSPI

// /*==================== LCD1 引脚定义 ====================*/
#define LCD1_PIN_SCLK      GPIO_NUM_13
#define LCD1_PIN_MOSI      GPIO_NUM_14
#define LCD1_PIN_CS        GPIO_NUM_NC // 不需要片选
#define LCD1_PIN_DC        GPIO_NUM_12
#define LCD1_PIN_RST       GPIO_NUM_15
#define LCD1_PIN_BL        GPIO_NUM_16 // 共用背光

// /*==================== LCD2 引脚定义 ====================*/
#define LCD2_PIN_SCLK      GPIO_NUM_29
#define LCD2_PIN_MOSI      GPIO_NUM_30
#define LCD2_PIN_CS        GPIO_NUM_NC // 不需要片选
#define LCD2_PIN_DC        GPIO_NUM_28
#define LCD2_PIN_RST       GPIO_NUM_31
#define LCD2_PIN_BL        GPIO_NUM_16  

/* 单个物理显示设备 */
typedef struct {
    /* ---- 硬件层 ---- */
    spi_host_device_t         host;
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t    panel;

    /* ---- Ping-Pong 双缓冲 (启动时预分配, DMA-safe) ---- */
    uint16_t *chunk_buf[2];       /* [0]=A, [1]=B */
    uint8_t   write_idx;          /* CPU 当前写入的 buffer 索引 */

    /* ---- DMA 同步 ---- */
    SemaphoreHandle_t dma_done;   /* 二值信号量：DMA 完成回调中 Give */
    volatile bool     dma_busy;   /* true = 有 DMA 传输正在进行 */

    /* ---- LVGL 关联 ---- */
    lv_display_t *lv_disp;       /* LVGL 显示对象 */

    /* ---- 引脚 ---- */
    int pin_sclk, pin_mosi, pin_cs, pin_dc, pin_rst;
} display_dev_t;

/* API */
void display_hal_init(void);
void display_wait_dma_done(int dev_idx);
void display_send_chunk(int dev_idx, int y_start, int y_end, uint16_t *data);
uint16_t *display_get_write_buf(int dev_idx);
void display_swap_buf(int dev_idx);
display_dev_t *display_get_dev(int dev_idx);
void display_set_panel_mirror_x(int dev_idx, bool mirror_x);

void lvgl_displays_init(void);
