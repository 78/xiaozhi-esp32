/* ============== display_hal.c ============== */
/*
 * 真正的异步双缓冲实现：
 *
 *  时序（理想情况）：
 *
 *  LVGL 渲染  [==render buf1==]          [==render buf0==]
 *  DMA 传输              [==DMA buf0==]              [==DMA buf1==]
 *                                   ↑
 *                        flush_ready 在 DMA 完成回调中发出
 *
 *  改动要点：
 *  1. lvgl_flush_cb 不再等待本次 DMA 完成，立刻返回
 *  2. on_color_trans_done ISR 通过 task notification 唤醒 lvgl_flush_ready_task
 *  3. lvgl_flush_ready_task 调用 lv_display_flush_ready，通知 LVGL 可写下一帧
 */

#include "display_hal.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"
#include <string.h>
#include "driver/gpio.h"
#include "freertos/task.h"

static const char *TAG = "disp_hal";

display_dev_t display_dev[NUM_DISPLAYS]; /* 左屏[0], 右屏[1] */

/* ------------------------------------------------------------------ */
/*  flush_ready 转发任务                                                */
/*                                                                      */
/*  ISR 不能直接调用 lv_display_flush_ready，                          */
/*  所以用一个专用任务接收 task notification，再转发给 LVGL。           */
/* ------------------------------------------------------------------ */
static TaskHandle_t s_flush_ready_task[NUM_DISPLAYS];

/*
 * flush_ready 任务：永久阻塞等待 task notification，
 * 收到后调用 lv_display_flush_ready 通知 LVGL。
 */
static void flush_ready_task(void *arg)
{
    int dev_idx = (int)(intptr_t)arg;

    for (;;) {
        /* 阻塞等待 ISR 发来的通知 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* 安全地调用 LVGL 接口（任务上下文，非 ISR） */
        lv_display_flush_ready(display_dev[dev_idx].lv_disp);
    }
}

/* ------------------------------------------------------------------ */
/*  DMA 完成回调（ISR 上下文）                                          */
/* ------------------------------------------------------------------ */
static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                           esp_lcd_panel_io_event_data_t *edata,
                                           void *user_ctx)
{
    display_dev_t *dev = (display_dev_t *)user_ctx;
    BaseType_t high_task_woken = pdFALSE;

    /* 标记 DMA 空闲，释放同步信号量（供 display_wait_dma_done 使用） */
    dev->dma_busy = false;
    xSemaphoreGiveFromISR(dev->dma_done, &high_task_woken);

    /*
     * ★ 通知 flush_ready 任务 ★
     *
     * 如果这个设备当前处于 LVGL 模式，就唤醒对应的 flush_ready 任务，
     * 让它去调用 lv_display_flush_ready。
     *
     * dev_idx 通过 dev 指针计算得出，无需额外存储。
     */
    int dev_idx = (int)(dev - display_dev); /* 0 或 1 */
    if (s_flush_ready_task[dev_idx] != NULL) {
        vTaskNotifyGiveFromISR(s_flush_ready_task[dev_idx], &high_task_woken);
    }

    return (high_task_woken == pdTRUE);
}

/* ------------------------------------------------------------------ */
/*  display_hal 公共 API                                                */
/* ------------------------------------------------------------------ */

/*
 * 等待指定设备的 DMA 传输完成
 */
void display_wait_dma_done(int dev_idx)
{
    display_dev_t *dev = &display_dev[dev_idx];
    if (dev->dma_busy) {
        if (xSemaphoreTake(dev->dma_done, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "SPI DMA timeout on dev %d!", dev_idx);
        }
    }
}

/*
 * 获取当前可写入的 buffer 指针
 */
uint16_t *display_get_write_buf(int dev_idx)
{
    display_dev_t *dev = &display_dev[dev_idx];
    return dev->chunk_buf[dev->write_idx];
}

/*
 * 发送一块数据到 LCD，并切换 buffer（直接渲染模式使用）
 *
 * 流程：
 *  1. 等待上一次 DMA 完成（保护 DMA 正在读取的 buffer）
 *  2. 发起新的 DMA 传输
 *  3. 切换 write_idx，CPU 立刻可以写另一个 buffer
 *     → CPU 写新 buffer 与 DMA 传输旧 buffer 并行
 */
void display_send_chunk(int dev_idx, int y_start, int y_end, uint16_t *data)
{
    display_dev_t *dev = &display_dev[dev_idx];

    /* 确保上一次 DMA 已经完成，再发起新传输 */
    display_wait_dma_done(dev_idx);

    dev->dma_busy = true;
    esp_lcd_panel_draw_bitmap(dev->panel, 0, y_start, LCD_H_RES, y_end, data);

    /* 切换 buffer：CPU 现在可以安全地写另一个 buffer */
    dev->write_idx ^= 1;
}

void display_set_panel_mirror_x(int dev_idx, bool mirror_x)
{
    display_dev_t *dev = &display_dev[dev_idx];

    display_wait_dma_done(dev_idx);
    dev->dma_busy = true;

    esp_lcd_panel_mirror(dev->panel, mirror_x, false);

    char data[2] = {0x00, 0x00};
    esp_lcd_panel_draw_bitmap(dev->panel, 0, 0, 1, 1, data);

    dev->write_idx ^= 1;
}

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
/* ------------------------------------------------------------------ */

static void init_one_display(display_dev_t *dev)
{
    ESP_LOGI(TAG, "Init display on SPI host %d", dev->host);

    /* 1. SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = dev->pin_sclk,
        .mosi_io_num     = dev->pin_mosi,
        .miso_io_num     = -1,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = CHUNK_BUF_BYTES,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(dev->host, &bus_cfg, SPI_DMA_CH_AUTO));

    /* 2. Panel IO —— 注册 DMA 完成回调 */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num          = dev->pin_cs,
        .dc_gpio_num          = dev->pin_dc,
        .spi_mode             = 0,
        .pclk_hz              = 80 * 1000 * 1000,
        .trans_queue_depth    = 2,
        .on_color_trans_done  = on_color_trans_done, /* ★ 关键回调 ★ */
        .user_ctx             = dev,
        .lcd_cmd_bits         = 8,
        .lcd_param_bits       = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)dev->host, &io_cfg, &dev->io));

    /* 3. GC9A01 Panel */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num   = dev->pin_rst,
        .rgb_ele_order    = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel   = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(dev->io, &panel_cfg, &dev->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(dev->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(dev->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(dev->panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(dev->panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(dev->panel, true));

    /* 4. 预分配 Ping-Pong Buffer */
    for (int i = 0; i < 2; i++) {
        dev->chunk_buf[i] = (uint16_t *)heap_caps_malloc(
            CHUNK_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

        if (!dev->chunk_buf[i]) {
            dev->chunk_buf[i] = (uint16_t *)heap_caps_malloc(
                CHUNK_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
        }
        assert(dev->chunk_buf[i] && "Chunk buffer alloc failed!");
        memset(dev->chunk_buf[i], 0, CHUNK_BUF_BYTES);
    }
    dev->write_idx = 0;

    /* 5. DMA 同步信号量 */
    dev->dma_done = xSemaphoreCreateBinary();
    assert(dev->dma_done);
    xSemaphoreGive(dev->dma_done); /* 初始状态：空闲 */
    dev->dma_busy = false;

    ESP_LOGI(TAG, "Display on host %d ready. Buf: %p / %p",
             dev->host, dev->chunk_buf[0], dev->chunk_buf[1]);
}

void display_hal_init(void)
{
    display_dev[1] = (display_dev_t){
        .host      = LCD1_HOST,
        .pin_sclk  = LCD1_PIN_SCLK, .pin_mosi = LCD1_PIN_MOSI,
        .pin_cs    = LCD1_PIN_CS,   .pin_dc   = LCD1_PIN_DC,
        .pin_rst   = LCD1_PIN_RST,
    };
    display_dev[0] = (display_dev_t){
        .host      = LCD2_HOST,
        .pin_sclk  = LCD2_PIN_SCLK, .pin_mosi = LCD2_PIN_MOSI,
        .pin_cs    = LCD2_PIN_CS,   .pin_dc   = LCD2_PIN_DC,
        .pin_rst   = LCD2_PIN_RST,
    };

    /* 背光 */
    // gpio_config_t bl_cfg = {
    //     .mode         = GPIO_MODE_OUTPUT,
    //     .pin_bit_mask = 1ULL << LCD2_PIN_BL,
    // };
    // gpio_config(&bl_cfg);
    // gpio_set_level(LCD2_PIN_BL, 1);

    init_one_display(&display_dev[0]);
    init_one_display(&display_dev[1]);
}

display_dev_t *display_get_dev(int dev_idx)
{
    assert(dev_idx >= 0 && dev_idx < NUM_DISPLAYS);
    return &display_dev[dev_idx];
}

/* ================================================================== */
/*  LVGL 层                                                             */
/* ================================================================== */

typedef struct {
    int dev_idx;
    lv_display_t *disp;
} lvgl_flush_ctx_t;

static lvgl_flush_ctx_t s_flush_ctx[NUM_DISPLAYS];

/*
 * ★ 真正异步的 flush 回调 ★
 *
 * 时序：
 *  1. 等待上一次 DMA 完成（保护上一帧 buffer 不被覆写）
 *  2. 对当前帧做 bswap
 *  3. 发起新的 DMA 传输，立刻返回（不等完成）
 *  4. DMA 完成后，ISR → flush_ready_task → lv_display_flush_ready
 *     → LVGL 开始渲染下一帧到另一个 buffer
 *
 *  CPU 渲染下一帧  与  DMA 传输本帧  完全并行。
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    lvgl_flush_ctx_t *ctx = (lvgl_flush_ctx_t *)lv_display_get_user_data(disp);
    display_dev_t    *dev = &display_dev[ctx->dev_idx];

    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;

    /* bswap：硬件字节序转换 */
    uint32_t  size = (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    uint16_t *p    = (uint16_t *)color_p;

    uint32_t i = 0;
    for (; i + 3 < size; i += 4) {
        p[i]     = __builtin_bswap16(p[i]);
        p[i + 1] = __builtin_bswap16(p[i + 1]);
        p[i + 2] = __builtin_bswap16(p[i + 2]);
        p[i + 3] = __builtin_bswap16(p[i + 3]);
    }
    for (; i < size; i++) {
        p[i] = __builtin_bswap16(p[i]);
    }

    /*
     * 等待上一次 DMA 完成。
     *
     * 必要性：LVGL 给我们的 color_p 可能是上一帧 DMA 还在读的 buffer。
     * 等它读完，我们才能放心发起新传输（DMA 会从 color_p 读新数据）。
     *
     * 注意：这里只等上一次，不等本次——本次由 ISR 回调通知。
     */
    display_wait_dma_done(ctx->dev_idx);

    /* 发起 DMA 传输，★ 不等待完成 ★ */
    dev->dma_busy = true;
    esp_lcd_panel_draw_bitmap(dev->panel, x1, y1, x2, y2, color_p);

    /*
     * 直接返回，不调用 lv_display_flush_ready。
     * flush_ready 将在 DMA 完成后由 flush_ready_task 调用。
     *
     * 此时 LVGL 处于"等待 flush 完成"状态，不会写这个 buffer，
     * 但 DMA 传输期间 LVGL 可以做其他不涉及 flush 的工作。
     */
}

/* ------------------------------------------------------------------ */

static esp_timer_handle_t s_lv_tick_timer;

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(10);
}

void lvgl_displays_init(void)
{
    lv_init();

    const esp_timer_create_args_t tcfg = {
        .callback = lv_tick_cb,
        .name     = "lv_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&tcfg, &s_lv_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_lv_tick_timer, 10000));

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        display_dev_t *dev = &display_dev[i];

        /* ★ 为每个屏幕创建 flush_ready 转发任务 ★ */
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "flush_rdy%d", i);
        xTaskCreate(flush_ready_task,
                    task_name,
                    512,              
                    (void *)(intptr_t)i,
                    configMAX_PRIORITIES - 1, /* 高优先级，确保及时通知 LVGL */
                    &s_flush_ready_task[i]);

        /* 创建 LVGL display */
        lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);

        s_flush_ctx[i].dev_idx = i;
        s_flush_ctx[i].disp    = disp;
        lv_display_set_user_data(disp, &s_flush_ctx[i]);
        lv_display_set_flush_cb(disp, lvgl_flush_cb);

        /*
         * 双缓冲：chunk_buf[0] 和 chunk_buf[1] 交替使用。
         * LVGL 写 buf A 时，DMA 传输 buf B；DMA 完成后角色互换。
         *
         * LV_DISPLAY_RENDER_MODE_PARTIAL：LVGL 只渲染脏区域，
         * 配合双缓冲可以做到渲染与传输完全并行。
         */
        lv_display_set_buffers(disp,
                               dev->chunk_buf[0],
                               dev->chunk_buf[1],
                               CHUNK_BUF_BYTES,
                               LV_DISPLAY_RENDER_MODE_PARTIAL);

        dev->lv_disp = disp;

        lv_disp_set_default(disp);
        lv_theme_t *theme = lv_theme_default_init(
            lv_disp_get_default(),
            lv_palette_main(LV_PALETTE_BLUE),
            lv_palette_main(LV_PALETTE_RED),
            true,
            LV_FONT_DEFAULT);
        lv_disp_set_theme(lv_disp_get_default(), theme);

        ESP_LOGI(TAG, "LVGL display %d created (async double-buffer)", i);
    }
}