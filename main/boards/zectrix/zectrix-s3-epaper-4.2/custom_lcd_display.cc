#include <stdio.h>
#include <cstring>
#include <algorithm>

#include <esp_log.h>
#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board.h"
#include "config.h"
#include "esp_lvgl_port.h"
#include "settings.h"
#include "custom_lcd_display.h"
#include "common/sleep_manager.h"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_puhui_basic_30_4);

#define TAG "CustomLcdDisplay"
static constexpr uint32_t kDisplayKickMs = 1000;

#ifndef EXAMPLE_LCD_WIDTH
#define EXAMPLE_LCD_WIDTH  400
#endif
#ifndef EXAMPLE_LCD_HEIGHT
#define EXAMPLE_LCD_HEIGHT 300
#endif

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * BYTES_PER_PIXEL)

#undef ESP_LOGI
#define ESP_LOGI(tag, fmt, ...) ((void)0)

static inline int rect_area(const Rect &r) {
    return (r.w > 0 && r.h > 0) ? (r.w * r.h) : 0;
}
static inline Rect rect_union(const Rect &a, const Rect &b) {
    if (rect_area(a) == 0) return b;
    if (rect_area(b) == 0) return a;
    int x1 = std::min(a.x, b.x);
    int y1 = std::min(a.y, b.y);
    int x2 = std::max(a.x + a.w, b.x + b.w);
    int y2 = std::max(a.y + a.h, b.y + b.h);
    return { x1, y1, x2 - x1, y2 - y1 };
}
static inline Rect clamp_rect(const Rect &r, int W, int H) {
    int x1 = std::max(0, r.x);
    int y1 = std::max(0, r.y);
    int x2 = std::min(W, r.x + r.w);
    int y2 = std::min(H, r.y + r.h);
    return { x1, y1, x2 - x1, y2 - y1 };
}
static inline Rect align_x8(const Rect &r) {
    Rect out = r;
    int x0 = (out.x / 8) * 8;
    int x1 = ((out.x + out.w + 7) / 8) * 8;
    out.x = x0;
    out.w = x1 - x0;
    return out;
}

static inline bool rgb565_is_white(uint16_t c, uint8_t thr) {
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5)  & 0x3F;
    uint8_t b5 = (c)       & 0x1F;

    uint8_t R = (uint8_t)((r5 * 255 + 15) / 31);
    uint8_t G = (uint8_t)((g6 * 255 + 31) / 63);
    uint8_t B = (uint8_t)((b5 * 255 + 15) / 31);

    uint16_t y = (uint16_t)((77 * R + 150 * G + 29 * B) >> 8);
    return y >= thr;
}

static inline void set_pixel_1bpp(uint8_t *fb, int width, int x, int y, bool white) {
    uint16_t bytes_per_row = (width + 7) >> 3;
    uint32_t index = (uint32_t)y * bytes_per_row + (uint32_t)(x >> 3);
    uint8_t  bit   = (uint8_t)(7 - (x & 0x07));
    uint8_t  mask  = (uint8_t)(1U << bit);
    if (white) fb[index] |= mask;
    else       fb[index] &= (uint8_t)~mask;
}

void CustomLcdDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    assert(disp && area && color_p);
    CustomLcdDisplay *driver = (CustomLcdDisplay *)lv_display_get_user_data(disp);
    assert(driver);

    if (driver->dirty_mutex) {
        xSemaphoreTake(driver->dirty_mutex, portMAX_DELAY);
    }
    const uint16_t *src = (const uint16_t *)color_p;

    int x1 = std::max(0, (int)area->x1);
    int y1 = std::max(0, (int)area->y1);
    int x2 = std::min(driver->Width - 1,  (int)area->x2);
    int y2 = std::min(driver->Height - 1, (int)area->y2);

    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    int src_w = (area->x2 - area->x1 + 1);

    for (int yy = 0; yy < h; yy++) {
        int y = y1 + yy;
        const uint16_t *row = src + (yy + (y1 - area->y1)) * src_w + (x1 - area->x1);
        for (int xx = 0; xx < w; xx++) {
            int x = x1 + xx;
            bool white = rgb565_is_white(row[xx], driver->bw_threshold);
            set_pixel_1bpp(driver->buffer, driver->Width, x, y, white);
        }
    }

    Rect r = { x1, y1, w, h };
    r = clamp_rect(align_x8(r), driver->Width, driver->Height);
    if (rect_area(r) > 0) {
        driver->dirty = rect_union(driver->dirty, r);
        driver->pending = true;
        driver->refresh_in_progress = true;
        driver->UpdateDisplayBusyLocked();
        uint32_t kick_ms = kDisplayKickMs;
        if (driver->next_kick_ms_ > 0) {
            kick_ms = driver->next_kick_ms_;
            driver->next_kick_ms_ = 0;
        }
        sm_kick(kick_ms, "display_flush");

        if (driver->refresh_task) {
            xTaskNotifyGive(driver->refresh_task);
        }
    }

    if (driver->dirty_mutex) {
        xSemaphoreGive(driver->dirty_mutex);
    }

    lv_disp_flush_ready(disp);
}

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y,
                                   bool mirror_x, bool mirror_y, bool swap_xy, custom_lcd_spi_t _lcd_spi_data) :
    LcdDisplay(panel_io, panel, width, height),
    lcd_spi_data(_lcd_spi_data),
    Width(width), Height(height) {

    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    lvgl_port_lock(0);

    buffer = (uint8_t *)heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(buffer);
    memset(buffer, 0xFF, lcd_spi_data.buffer_len);

    prev_buffer = (uint8_t *)heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(prev_buffer);
    memset(prev_buffer, 0xFF, lcd_spi_data.buffer_len);

    tx_buf = (uint8_t *)heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(tx_buf);
    memset(tx_buf, 0xFF, lcd_spi_data.buffer_len);

    display_ = lv_display_create(width, height);
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t *buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, NULL, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    bw_threshold       = 200;

    sample_interval_ms = 300;
    last_sample_tick = 0;

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();

    EPD_Clear();
    memcpy(prev_buffer, buffer, lcd_spi_data.buffer_len);
    EPD_Display();

    dirty_mutex = xSemaphoreCreateMutex();
    assert(dirty_mutex);
    start_refresh_task();

    lvgl_port_unlock();

    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    ESP_LOGI(TAG, "ui start");
    SetupUI();
}

CustomLcdDisplay::~CustomLcdDisplay() {
    stop_refresh_task();
    if (dirty_mutex) {
        vSemaphoreDelete(dirty_mutex);
        dirty_mutex = nullptr;
    }
}

struct FrameDiffResult {
    size_t diff_bits;
    float diff_ratio;
};

static FrameDiffResult analyze_frame_diff(
    const uint8_t* prev_buffer,
    const uint8_t* tx_buf,
    int width,
    int height
) {
    FrameDiffResult result = {};
    result.diff_bits = 0;
    result.diff_ratio = 0.0f;

    if (!prev_buffer || !tx_buf || width <= 0 || height <= 0) {
        return result;
    }

    const int bytes_per_row = (width + 7) >> 3;
    const size_t total_bytes = bytes_per_row * height;
    const size_t total_bits = total_bytes * 8;

    for (int y = 0; y < height; ++y) {
        const uint8_t* prow = prev_buffer + y * bytes_per_row;
        const uint8_t* crow = tx_buf + y * bytes_per_row;
        for (int xb = 0; xb < bytes_per_row; ++xb) {
            uint8_t x = (uint8_t)(prow[xb] ^ crow[xb]);
            if (x != 0) {
                result.diff_bits += (size_t)__builtin_popcount((unsigned)x);
            }
        }
    }

    result.diff_ratio = (total_bits > 0) ? (float)result.diff_bits / (float)total_bits : 0.0f;

    return result;
}

void CustomLcdDisplay::start_refresh_task() {
    if (refresh_task) return;
    xTaskCreatePinnedToCore(refresh_task_entry, "epd_refresh", 4096, this, 3, &refresh_task, 1);
}

void CustomLcdDisplay::stop_refresh_task() {
    if (!refresh_task) return;
    TaskHandle_t t = refresh_task;
    refresh_task = nullptr;
    vTaskDelete(t);
}

void CustomLcdDisplay::UpdateDisplayBusyLocked() {
    const bool busy = pending || urgent_refresh || force_full_refresh_ || refresh_in_progress;
    sm_set_busy(SleepBusySrc::Display, busy);
}

bool CustomLcdDisplay::CheckRefreshIdleLocked() {
    const bool busy = pending || urgent_refresh || force_full_refresh_ || refresh_in_progress;
    if (busy) {
        refresh_busy_seen_ = true;
        return false;
    }
    if (!refresh_busy_seen_) {
        return false;
    }
    refresh_busy_seen_ = false;
    return true;
}

bool CustomLcdDisplay::IsRefreshPending() {
    if (dirty_mutex) {
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
    }
    const bool busy = pending || urgent_refresh || force_full_refresh_ || refresh_in_progress;
    if (dirty_mutex) {
        xSemaphoreGive(dirty_mutex);
    }
    return busy;
}

void CustomLcdDisplay::RequestUrgentRefresh() {
    if (dirty_mutex) {
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
    }
    urgent_refresh = true;
    refresh_in_progress = true;
    const uint32_t kick_ms = (next_kick_ms_ > 0) ? next_kick_ms_ : kDisplayKickMs;
    next_kick_ms_ = 0;
    UpdateDisplayBusyLocked();
    if (dirty_mutex) {
        xSemaphoreGive(dirty_mutex);
    }
    sm_kick(kick_ms, "display_urgent");
    if (refresh_task) {
        xTaskNotifyGive(refresh_task);
    }
}

void CustomLcdDisplay::RequestUrgentFullRefresh() {
    if (dirty_mutex) {
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
    }
    urgent_refresh = true;
    force_full_refresh_ = true;
    refresh_in_progress = true;
    const uint32_t kick_ms = (next_kick_ms_ > 0) ? next_kick_ms_ : kDisplayKickMs;
    next_kick_ms_ = 0;
    UpdateDisplayBusyLocked();
    if (dirty_mutex) {
        xSemaphoreGive(dirty_mutex);
    }
    sm_kick(kick_ms, "display_urgent");
    if (refresh_task) {
        xTaskNotifyGive(refresh_task);
    }
}

void CustomLcdDisplay::SetOnRefreshIdle(std::function<void()> cb) {
    if (dirty_mutex) {
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
    }
    on_refresh_idle_ = std::move(cb);
    if (dirty_mutex) {
        xSemaphoreGive(dirty_mutex);
    }
}

void CustomLcdDisplay::SetNextKickMs(uint32_t kick_ms) {
    if (dirty_mutex) {
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
    }
    next_kick_ms_ = kick_ms;
    if (dirty_mutex) {
        xSemaphoreGive(dirty_mutex);
    }
}

void CustomLcdDisplay::refresh_task_entry(void *arg) {
    CustomLcdDisplay *d = (CustomLcdDisplay *)arg;
    d->refresh_task_loop();
}

void CustomLcdDisplay::refresh_task_loop() {
    int partial_since_full = 0;
    int tiny_diff_streak = 0;
    size_t tiny_diff_accum_bits = 0;
    TickType_t tiny_diff_first_tick = 0;

    uint32_t stat_refresh = 0;
    uint32_t stat_full = 0;
    uint32_t stat_partial = 0;
    uint32_t stat_urgent = 0;
    uint32_t stat_skip_throttle = 0;
    uint32_t stat_skip_nodiff = 0;
    uint32_t stat_skip_tiny = 0;
    uint32_t stat_tiny_forced = 0;
    TickType_t last_stat_tick = 0;

    const TickType_t kDebounceTicks = pdMS_TO_TICKS(50);
    const TickType_t kUrgentDebounceTicks = pdMS_TO_TICKS(30);
    const float kMinDiffBitRatio = 0.001f;
    const float kForceFullDiffRatio = 0.30f;
    const int kTinyMaxStreak = 4;
    const size_t kTinyMaxAccumBits = 64 * 8;
    const TickType_t kTinyMaxHoldTicks = pdMS_TO_TICKS(1200);
    const TickType_t kStatPeriodTicks = pdMS_TO_TICKS(3000);

    auto maybe_log_stats = [&](TickType_t now_tick) {
        if (last_stat_tick == 0) {
            last_stat_tick = now_tick;
            return;
        }
        if ((now_tick - last_stat_tick) >= kStatPeriodTicks) {
            ESP_LOGI(TAG,
                     "[REFRESH] Stat 3s: refresh=%u (full=%u, partial=%u, urgent=%u), "
                     "skip(throttle=%u, nodiff=%u, tiny=%u, tiny_forced=%u)",
                     (unsigned)stat_refresh, (unsigned)stat_full, (unsigned)stat_partial,
                     (unsigned)stat_urgent, (unsigned)stat_skip_throttle,
                     (unsigned)stat_skip_nodiff, (unsigned)stat_skip_tiny,
                     (unsigned)stat_tiny_forced);
            last_stat_tick = now_tick;
        }
    };

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        TickType_t now = xTaskGetTickCount();

        bool urgent = false;
        bool force_full = false;
        Rect r = {0, 0, 0, 0};

        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
        if (urgent_refresh) {
            urgent = true;
            urgent_refresh = false;
        }
        if (force_full_refresh_) {
            force_full = true;
            force_full_refresh_ = false;
        }
        if (pending && rect_area(dirty) > 0) {
            r = dirty;
            dirty = {0, 0, 0, 0};
            pending = false;
        }
        if (urgent || rect_area(r) > 0) {
            refresh_in_progress = true;
        }
        UpdateDisplayBusyLocked();
        (void)CheckRefreshIdleLocked();
        xSemaphoreGive(dirty_mutex);

        TickType_t min_ticks = pdMS_TO_TICKS(sample_interval_ms);
        if (!urgent) {
            TickType_t elapsed = (last_sample_tick == 0) ? min_ticks : (now - last_sample_tick);
            if (elapsed < min_ticks) {
                stat_skip_throttle++;
                maybe_log_stats(now);
                TickType_t wait_ticks = min_ticks - elapsed;
                vTaskDelay(wait_ticks > 0 ? wait_ticks : 1);
                continue;
            }
        }

        TickType_t debounce_ticks = urgent ? kUrgentDebounceTicks : kDebounceTicks;
        TickType_t t0 = xTaskGetTickCount();
        while (debounce_ticks > 0 && (xTaskGetTickCount() - t0) < debounce_ticks) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));
            xSemaphoreTake(dirty_mutex, portMAX_DELAY);
            if (pending && rect_area(dirty) > 0) {
                r = rect_union(r, dirty);
                dirty = {0, 0, 0, 0};
                pending = false;
            }
            xSemaphoreGive(dirty_mutex);
        }

        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
        memcpy(tx_buf, buffer, lcd_spi_data.buffer_len);
        xSemaphoreGive(dirty_mutex);
        last_sample_tick = xTaskGetTickCount();

        FrameDiffResult result = analyze_frame_diff(prev_buffer, tx_buf, Width, Height);

        if (result.diff_bits == 0 && !force_full) {
            tiny_diff_streak = 0;
            tiny_diff_accum_bits = 0;
            tiny_diff_first_tick = 0;
            stat_skip_nodiff++;
            maybe_log_stats(last_sample_tick);
            bool fire_idle_cb = false;
            xSemaphoreTake(dirty_mutex, portMAX_DELAY);
            urgent_refresh = false;
            refresh_in_progress = false;
            UpdateDisplayBusyLocked();
            fire_idle_cb = CheckRefreshIdleLocked();
            xSemaphoreGive(dirty_mutex);
            if (fire_idle_cb && on_refresh_idle_) {
                on_refresh_idle_();
            }
            vTaskDelay(1);
            continue;
        }

        if (!urgent && !force_full && result.diff_ratio < kMinDiffBitRatio) {
            if (tiny_diff_streak == 0) {
                tiny_diff_first_tick = last_sample_tick;
            }
            tiny_diff_streak++;
            tiny_diff_accum_bits += result.diff_bits;

            bool force_due = (tiny_diff_streak >= kTinyMaxStreak) ||
                             (tiny_diff_accum_bits >= kTinyMaxAccumBits) ||
                             (last_sample_tick - tiny_diff_first_tick >= kTinyMaxHoldTicks);
            if (!force_due) {
                stat_skip_tiny++;
                maybe_log_stats(last_sample_tick);
                vTaskDelay(1);
                continue;
            }
            stat_tiny_forced++;
        } else {
            tiny_diff_streak = 0;
            tiny_diff_accum_bits = 0;
            tiny_diff_first_tick = 0;
        }

        const size_t total_bytes = lcd_spi_data.buffer_len;
        const size_t total_bits = total_bytes * 8;

        bool should_full = !prev_buffer_synced || !prev_buffer;
        if (!should_full && force_full) {
            should_full = true;
        }
        if (!should_full && result.diff_ratio >= kForceFullDiffRatio) {
            should_full = true;
        }
        if (!should_full && partial_since_full >= 10) {
            should_full = true;
        }

        stat_refresh++;
        if (urgent) {
            stat_urgent++;
        }

        if (should_full) {
            stat_full++;
            EPD_Init();
            EPD_Display();

            memcpy(prev_buffer, tx_buf, lcd_spi_data.buffer_len);
            prev_buffer_synced = true;
            partial_since_full = 0;
        } else {
            stat_partial++;
            EPD_Init();
            EPD_DisplayPart();
            memcpy(prev_buffer, tx_buf, lcd_spi_data.buffer_len);
            prev_buffer_synced = true;
            partial_since_full++;
        }
        xSemaphoreTake(dirty_mutex, portMAX_DELAY);
        refresh_in_progress = false;
        UpdateDisplayBusyLocked();
        bool fire_idle_cb = CheckRefreshIdleLocked();
        xSemaphoreGive(dirty_mutex);
        if (fire_idle_cb && on_refresh_idle_) {
            on_refresh_idle_();
        }
        tiny_diff_streak = 0;
        tiny_diff_accum_bits = 0;
        tiny_diff_first_tick = 0;
        maybe_log_stats(xTaskGetTickCount());
    }
}

void CustomLcdDisplay::spi_gpio_init() {
    int rst  = lcd_spi_data.rst;
    int cs   = lcd_spi_data.cs;
    int dc   = lcd_spi_data.dc;
    int busy = lcd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode         = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void CustomLcdDisplay::spi_port_init() {
    int              mosi     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;

    if (spi && spi_bus_inited) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_remove_device(spi));
        spi = nullptr;
    }
    if (spi_bus_inited) {
        esp_err_t free_ret = spi_bus_free((spi_host_device_t)spi_host);
        if (free_ret != ESP_OK && free_ret != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(free_ret);
        }
        spi_bus_inited = false;
    }

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = -1;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = lcd_spi_data.buffer_len;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 40 * 1000 * 1000;
    devcfg.mode                          = 0;
    devcfg.queue_size                    = 7;

    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)spi_host, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device((spi_host_device_t)spi_host, &devcfg, &spi));
    spi_bus_inited = true;
}

void CustomLcdDisplay::spi_port_rx_init() {
    int              miso     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;

    if (spi && spi_bus_inited) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_remove_device(spi));
        spi = nullptr;
    }
    if (spi_bus_inited) {
        esp_err_t free_ret = spi_bus_free((spi_host_device_t)spi_host);
        if (free_ret != ESP_OK && free_ret != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(free_ret);
        }
        spi_bus_inited = false;
    }

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = miso;
    buscfg.mosi_io_num      = -1;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = lcd_spi_data.buffer_len;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 8 * 1000 * 1000;
    devcfg.mode                          = 0;
    devcfg.queue_size                    = 7;

    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)spi_host, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device((spi_host_device_t)spi_host, &devcfg, &spi));
    spi_bus_inited = true;
}

void CustomLcdDisplay::read_busy() {
    int busy = lcd_spi_data.busy;
    while (gpio_get_level((gpio_num_t)busy) == 0) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void CustomLcdDisplay::SPI_SendByte(uint8_t data) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &data;
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

uint8_t CustomLcdDisplay::SPI_RecvByte() {
    uint8_t rx = 0;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.rx_buffer = &rx;
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
    return rx;
}

uint8_t CustomLcdDisplay::EPD_RecvData() {
    unsigned char data = 0;
    spi_port_rx_init();
    set_dc_1();
    set_cs_0();
    data = SPI_RecvByte();
    set_cs_1();
    spi_port_init();
    return data;
}

void CustomLcdDisplay::EPD_SendData(uint8_t data) {
    set_dc_1();
    set_cs_0();
    SPI_SendByte(data);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SendCommand(uint8_t command) {
    set_dc_0();
    set_cs_0();
    SPI_SendByte(command);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(uint8_t *buf, int len) {
    set_dc_1();
    set_cs_0();
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buf;
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(const uint8_t *buf, int len) {
    set_dc_1();
    set_cs_0();
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buf;
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x04);
    read_busy();

    EPD_SendCommand(0x12);
    EPD_SendData(0x00);
    read_busy();

    EPD_SendCommand(0x02);
    EPD_SendData(0x00);
    read_busy();
    EPD_PowerOff();
}

void CustomLcdDisplay::EPD_PowerOn() {
    // 已经在 BoardPowerBsp 中打开了电源，这里不需要重复操作
    // gpio_hold_dis((gpio_num_t)lcd_spi_data.power);
    // gpio_set_level((gpio_num_t)lcd_spi_data.power, 1);
    // gpio_hold_en((gpio_num_t)lcd_spi_data.power);
}

void CustomLcdDisplay::EPD_PowerOff() {
    // 暂时不关闭电源，避免显示刷新后黑屏
    // gpio_hold_dis((gpio_num_t)lcd_spi_data.power);
    // gpio_set_level((gpio_num_t)lcd_spi_data.power, 0);
    // gpio_hold_en((gpio_num_t)lcd_spi_data.power);
}

void CustomLcdDisplay::EPD_Init() {
    EPD_PowerOn();
    vTaskDelay(pdMS_TO_TICKS(10));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(10));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(10));

    read_busy();

    EPD_SendCommand(0x00);
    EPD_SendData(0x2F);
    EPD_SendData(0x2E);

    EPD_SendCommand(0xE9);
    EPD_SendData(0x01);
    read_busy();
}

void CustomLcdDisplay::EPD_Clear() {
    memset(buffer, 0xFF, lcd_spi_data.buffer_len);
}

static inline void pack_1bpp_to_2683(uint8_t in, uint8_t& out0, uint8_t& out1) {
    uint8_t b0 = 0, b1 = 0;
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = (in >> (7 - i)) & 0x01;
        if (i < 4) {
            b0 |= bit << (8 - 2 * (i + 1));
        } else {
            b1 |= bit << (14 - 2 * i);
        }
    }
    out0 = b0;
    out1 = b1;
}

void CustomLcdDisplay::EPD_Display() {
    unsigned char temp1, tempvalue;

    const int bytes_per_row_1bpp = (Width + 7) >> 3;
    const int bytes_per_row_out  = bytes_per_row_1bpp * 2;

    std::vector<uint8_t> line(bytes_per_row_out);

    EPD_SendCommand(0x40);
    read_busy();

    temp1 = EPD_RecvData();

    if (temp1 <= 5)
        tempvalue = 232;
    else if (temp1 <= 10)
        tempvalue = 235;
    else if (temp1 <= 20)
        tempvalue = 238;
    else if (temp1 <= 30)
        tempvalue = 241;
    else if (temp1 <= 127)
        tempvalue = 244;
    else
        tempvalue = 232;

    EPD_SendCommand(0xE0);
    EPD_SendData(0x02);
    EPD_SendCommand(0xE6);
    EPD_SendData(tempvalue);

    EPD_SendCommand(0xA5);
    read_busy();
    vTaskDelay(pdMS_TO_TICKS(10));

    EPD_SendCommand(0x10);

    for (int y = 0; y < Height; y++) {
        const uint8_t* src = tx_buf + y * bytes_per_row_1bpp;
        uint8_t* dst = line.data();

        for (int xb = 0; xb < bytes_per_row_1bpp; ++xb) {
            uint8_t o0, o1;
            pack_1bpp_to_2683(src[xb], o0, o1);
            *dst++ = o0;
            *dst++ = o1;
        }

        writeBytes(line.data(), bytes_per_row_out);
    }

    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::bitInterleave(unsigned char bytes1, unsigned char bytes2) {
    unsigned short result = 0;
    for (int i = 0; i < 8; i++) {
        result |= ((bytes1 >> (7 - i)) & 1) << (2 * (7 - i) + 1);
        result |= ((bytes2 >> (7 - i)) & 1) << (2 * (7 - i));
        if (i == 3)
            EPD_SendData(result >> 8);
        if (i == 7)
            EPD_SendData(result);
    }
}

void CustomLcdDisplay::WRITE_WHITE_TO_HLINE() {
    unsigned int i, j;
    EPD_SendCommand(0x10);
    read_busy();
    for (i = 0; i < 300; i++) {
        for (j = 0; j < 50; j++) {
            if (j < 25)
                bitInterleave(0xFF, 0x00);
            else
                bitInterleave(0xFF, 0xFF);
        }
    }
}

void CustomLcdDisplay::WRITE_HLINE_TO_VLINE() {
    unsigned int i, j;
    EPD_SendCommand(0x10);
    read_busy();
    for (i = 0; i < 300; i++) {
        for (j = 0; j < 50; j++) {
            if (i < 150 && j < 25) {
                bitInterleave(0x00, 0x00);
            } else if (i >= 150 && j < 25) {
                bitInterleave(0x00, 0xFF);
            } else if (i < 150 && j >= 25) {
                bitInterleave(0xFF, 0x00);
            } else {
                bitInterleave(0xFF, 0xFF);
            }
        }
    }
}

void CustomLcdDisplay::WRITE_VLINE_TO_HLINE() {
    unsigned int i, j;
    EPD_SendCommand(0x10);
    read_busy();
    for (i = 0; i < 300; i++) {
        for (j = 0; j < 50; j++) {
            if (i < 150 && j < 25) {
                bitInterleave(0x00, 0x00);
            } else if (i >= 150 && j < 25) {
                bitInterleave(0xFF, 0x00);
            } else if (i < 150 && j >= 25) {
                bitInterleave(0x00, 0xFF);
            } else {
                bitInterleave(0xFF, 0xFF);
            }
        }
    }
}

void CustomLcdDisplay::EPD_DisplayPart() {
    const int bytes_per_row_1bpp = (Width + 7) >> 3;
    const int bytes_per_row_out  = bytes_per_row_1bpp * 2;

    std::vector<uint8_t> line(bytes_per_row_out);

    EPD_SendCommand(0x10);
    read_busy();

    for (int i = 0; i < Height; i++) {
        const uint8_t* prev_row = prev_buffer + i * bytes_per_row_1bpp;
        const uint8_t* tx_row   = tx_buf   + i * bytes_per_row_1bpp;

        for (int j = 0; j < bytes_per_row_1bpp; j++) {
            uint8_t b1 = prev_row[j];
            uint8_t b2 = tx_row[j];

            uint16_t result = 0;
            for (int k = 0; k < 8; k++) {
                const int src_bit = 7 - k;
                const int dst_bit0 = 2 * src_bit;
                const int dst_bit1 = 2 * src_bit + 1;

                result |= ((uint16_t)((b1 >> src_bit) & 1u)) << dst_bit1;
                result |= ((uint16_t)((b2 >> src_bit) & 1u)) << dst_bit0;
            }

            line[2 * j + 0] = (uint8_t)(result >> 8);
            line[2 * j + 1] = (uint8_t)(result & 0xFF);
        }

        writeBytes(line.data(), bytes_per_row_out);
    }

    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= (uint16_t)Width || y >= (uint16_t)Height) return;

    uint16_t bytes_per_row = (Width + 7) >> 3;
    uint32_t index = (uint32_t)y * bytes_per_row + (uint32_t)(x >> 3);
    uint8_t  bit   = (uint8_t)(7 - (x & 0x07));
    uint8_t  mask  = (uint8_t)(1U << bit);

    if (color == DRIVER_COLOR_WHITE) buffer[index] |= mask;
    else                             buffer[index] &= (uint8_t)~mask;
}

void CustomLcdDisplay::WriteRaw1bpp(int x, int y, int w, int h, const uint8_t* data, size_t len) {
    if (!data || !buffer || w <= 0 || h <= 0) return;

    const int src_bytes_per_row = (w + 7) >> 3;
    const size_t expected = (size_t)src_bytes_per_row * h;
    if (len < expected) {
        ESP_LOGW(TAG, "WriteRaw1bpp: data too short (%u < %u)", (unsigned)len, (unsigned)expected);
        return;
    }

    xSemaphoreTake(dirty_mutex, portMAX_DELAY);

    const int dst_bytes_per_row = (Width + 7) >> 3;
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= Height) continue;
        const uint8_t* src_row = data + row * src_bytes_per_row;
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= Width) continue;
            bool black = (src_row[col >> 3] >> (7 - (col & 7))) & 1;
            uint32_t idx = (uint32_t)dy * dst_bytes_per_row + (uint32_t)(dx >> 3);
            uint8_t mask = (uint8_t)(1U << (7 - (dx & 7)));
            if (black) {
                buffer[idx] &= ~mask;
            } else {
                buffer[idx] |= mask;
            }
        }
    }

    Rect r = clamp_rect(align_x8({x, y, w, h}), Width, Height);
    if (rect_area(r) > 0) {
        dirty = rect_union(dirty, r);
        pending = true;
        refresh_in_progress = true;
        UpdateDisplayBusyLocked();
        sm_kick(kDisplayKickMs, "display_raw1bpp");
        if (refresh_task) {
            xTaskNotifyGive(refresh_task);
        }
    }

    xSemaphoreGive(dirty_mutex);
}

static uint32_t utf8_next(const char** pp) {
    const uint8_t* p = (const uint8_t*)*pp;
    if (*p == 0) return 0;

    uint32_t c;
    int len;
    if (*p < 0x80)        { c = *p;         len = 1; }
    else if ((*p & 0xE0) == 0xC0) { c = *p & 0x1F; len = 2; }
    else if ((*p & 0xF0) == 0xE0) { c = *p & 0x0F; len = 3; }
    else if ((*p & 0xF8) == 0xF0) { c = *p & 0x07; len = 4; }
    else { *pp += 1; return 0xFFFD; }

    for (int i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) { *pp += 1; return 0xFFFD; }
        c = (c << 6) | (p[i] & 0x3F);
    }
    *pp += len;
    return c;
}

void CustomLcdDisplay::render_text_to_buffer(const char* text, int start_x, int start_y, const lv_font_t* font) {
    int cursor_x = start_x;
    int cursor_y = start_y;
    const char* p = text;

    while (*p) {
        uint32_t ch = utf8_next(&p);
        if (ch == 0) break;

        if (ch == '\n') {
            cursor_x = start_x;
            cursor_y += font->line_height;
            continue;
        }

        lv_font_glyph_dsc_t g = {};
        if (!lv_font_get_glyph_dsc(font, &g, ch, 0)) {
            cursor_x += font->line_height / 2;
            continue;
        }

        g.req_raw_bitmap = 1;
        const uint8_t* bitmap = (const uint8_t*)font->get_glyph_bitmap(&g, nullptr);
        g.req_raw_bitmap = 0;
        if (!bitmap) {
            cursor_x += g.adv_w;
            continue;
        }

        int gx = cursor_x + g.ofs_x;
        int gy = cursor_y + font->line_height - font->base_line - g.ofs_y - g.box_h;

        int row_bits = (g.stride > 0) ? (int)(g.stride * 8) : (int)g.box_w;

        for (int row = 0; row < g.box_h; row++) {
            for (int col = 0; col < g.box_w; col++) {
                int bit_idx = row * row_bits + col;
                bool pixel = (bitmap[bit_idx >> 3] >> (7 - (bit_idx & 7))) & 1;
                if (pixel) {
                    int px = gx + col;
                    int py = gy + row;
                    if (px >= 0 && px < Width && py >= 0 && py < Height) {
                        set_pixel_1bpp(buffer, Width, px, py, false);
                    }
                }
            }
        }

        cursor_x += g.adv_w;
    }
}

void CustomLcdDisplay::DrawTexts(const std::vector<TextItem>& texts, bool clear) {
    xSemaphoreTake(dirty_mutex, portMAX_DELAY);

    if (clear) {
        size_t buf_len = (size_t)((Width + 7) >> 3) * Height;
        memset(buffer, 0xFF, buf_len);
    }

    for (const auto& item : texts) {
        const lv_font_t* font = (item.size >= 20)
            ? &font_puhui_basic_30_4
            : &BUILTIN_TEXT_FONT;
        render_text_to_buffer(item.content.c_str(), item.x, item.y, font);
    }

    Rect r = clamp_rect(align_x8({0, 0, Width, Height}), Width, Height);
    dirty = rect_union(dirty, r);
    pending = true;
    refresh_in_progress = true;
    UpdateDisplayBusyLocked();
    sm_kick(kDisplayKickMs, "display_text");
    if (refresh_task) {
        xTaskNotifyGive(refresh_task);
    }

    xSemaphoreGive(dirty_mutex);
}

void CustomLcdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend) {
    EPD_SendCommand(0x44);
    EPD_SendData((Xstart >> 3) & 0xFF);
    EPD_SendData((Xend >> 3) & 0xFF);

    EPD_SendCommand(0x45);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E);
    EPD_SendData(Xstart & 0xFF);

    EPD_SendCommand(0x4F);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x04);
    read_busy();

    EPD_SendCommand(0x12);
    EPD_SendData(0x00);
    read_busy();

    EPD_SendCommand(0x02);
    EPD_SendData(0x00);
    read_busy();
    EPD_PowerOff();
}

void CustomLcdDisplay::EPD_SetFullWindowAndCounter() {
    EPD_SetWindows(0, 0, Width - 1, Height - 1);
    EPD_SetCursor(0, 0);
}

void CustomLcdDisplay::EPD_DisplayPartBaseImage() {
    EPD_Display();
}

void CustomLcdDisplay::EPD_Init_Partial() {
    EPD_Init();
}
