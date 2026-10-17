#include "nm_epd_display.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>

#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "esp_lvgl_port.h"

#define TAG "NmEpdDisplay"

// LVGL draws into a full-size RGB565 canvas in PSRAM.
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))

namespace {

constexpr uint8_t kBwThreshold = 200;         // luminance cut for 1bpp conversion
constexpr uint32_t kSpiClockHz = 10'000'000;  // conservative, matches the vendor demo
constexpr size_t kMaxTransferSize = 1024;     // internal-RAM DMA chunk size
constexpr uint32_t kBusyTimeoutMs = 15000;
constexpr int kMinRefreshIntervalMs = 300;  // panel-friendly throttle
constexpr int kQuietDebounceMs = 300;       // refresh once the UI is quiet for this long
constexpr int kMaxDebounceHoldMs = 1500;    // ... but never hold a pending update longer
constexpr int kMaxPartialPerFull = 10;      // periodic full refresh against ghosting
constexpr float kForceFullDiffRatio = 0.30f;
constexpr int kIdleFullRefreshDelayMs = 1500;  // run deferred full refresh after UI goes idle
constexpr size_t kChatTrimStep = 24;           // chars dropped per step when chat text overflows

inline bool Rgb565IsWhite(uint16_t c, uint8_t thr) {
    uint8_t r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
    uint8_t b = (uint8_t)((c & 0x1F) * 255 / 31);
    return (uint16_t)((77 * r + 150 * g + 29 * b) >> 8) >= thr;
}

inline void SetPixel1bpp(uint8_t* fb, int stride, int x, int y, bool white) {
    uint8_t& byte = fb[(uint32_t)y * stride + (x >> 3)];
    uint8_t mask = (uint8_t)(1U << (7 - (x & 7)));
    if (white) {
        byte |= mask;
    } else {
        byte &= (uint8_t)~mask;
    }
}

inline int RectArea(const NmEpdRect& r) { return (r.w > 0 && r.h > 0) ? r.w * r.h : 0; }

inline NmEpdRect RectUnion(const NmEpdRect& a, const NmEpdRect& b) {
    if (RectArea(a) == 0)
        return b;
    if (RectArea(b) == 0)
        return a;
    int x1 = std::min(a.x, b.x);
    int y1 = std::min(a.y, b.y);
    int x2 = std::max(a.x + a.w, b.x + b.w);
    int y2 = std::max(a.y + a.h, b.y + b.h);
    return {x1, y1, x2 - x1, y2 - y1};
}

// SSD1683 addresses RAM in byte (8 pixel) columns: snap X to byte boundaries.
inline NmEpdRect AlignRectX8(const NmEpdRect& r, int width, int height) {
    NmEpdRect out = r;
    int x0 = (out.x / 8) * 8;
    int x1 = std::min(((out.x + out.w + 7) / 8) * 8, width);
    out.x = std::max(0, x0);
    out.w = x1 - out.x;
    out.y = std::max(0, out.y);
    out.h = std::min(out.h, height - out.y);
    if (out.w <= 0 || out.h <= 0)
        return {0, 0, 0, 0};
    return out;
}

}  // namespace

NmEpdDisplay::NmEpdDisplay(int width, int height, const NmEpdPins& pins)
    : LcdDisplay(nullptr, nullptr, width, height),
      pins_(pins),
      width_(width),
      height_(height),
      frame_bytes_(width * height / 8) {
    ESP_LOGI(TAG, "Initialize SPI");
    SpiInit();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    frame_ = (uint8_t*)heap_caps_malloc(frame_bytes_, MALLOC_CAP_SPIRAM);
    prev_frame_ = (uint8_t*)heap_caps_malloc(frame_bytes_, MALLOC_CAP_SPIRAM);
    tx_frame_ = (uint8_t*)heap_caps_malloc(frame_bytes_, MALLOC_CAP_SPIRAM);
    // SPI DMA on ESP32-S3 cannot reliably source PSRAM; bounce every transfer
    // through this internal-RAM buffer instead (same approach as the vendor
    // demo driver).
    dma_buffer_ =
        (uint8_t*)heap_caps_malloc(kMaxTransferSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(frame_ && prev_frame_ && tx_frame_ && dma_buffer_);
    memset(frame_, 0xFF, frame_bytes_);
    memset(prev_frame_, 0xFF, frame_bytes_);
    memset(tx_frame_, 0xFF, frame_bytes_);

    display_ = lv_display_create(width_, height_);
    assert(display_);
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    const size_t lv_buf_size = (size_t)width_ * height_ * BYTES_PER_PIXEL;
    uint8_t* lv_buf = (uint8_t*)heap_caps_malloc(lv_buf_size, MALLOC_CAP_SPIRAM);
    assert(lv_buf);
    lv_display_set_buffers(display_, lv_buf, NULL, lv_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "EPD panel init");
    InitPanel();
    // Establish a known-white baseline in both controller RAM banks.
    RefreshFull(frame_);
    memcpy(prev_frame_, frame_, frame_bytes_);
    prev_valid_ = true;

    dirty_mutex_ = xSemaphoreCreateMutex();
    assert(dirty_mutex_);
    StartRefreshTask();

    lvgl_port_unlock();

    // Note: SetupUI() is called by Application::Initialize() once the display
    // is fully initialized.
}

NmEpdDisplay::~NmEpdDisplay() {
    if (refresh_task_) {
        TaskHandle_t t = refresh_task_;
        refresh_task_ = nullptr;
        vTaskDelete(t);
    }
    if (dirty_mutex_) {
        vSemaphoreDelete(dirty_mutex_);
        dirty_mutex_ = nullptr;
    }
}

void NmEpdDisplay::SpiInit() {
    gpio_config_t out_conf = {};
    out_conf.intr_type = GPIO_INTR_DISABLE;
    out_conf.mode = GPIO_MODE_OUTPUT;
    out_conf.pin_bit_mask = (1ULL << pins_.cs) | (1ULL << pins_.dc) | (1ULL << pins_.rst);
    out_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&out_conf));

    gpio_config_t in_conf = {};
    in_conf.intr_type = GPIO_INTR_DISABLE;
    in_conf.mode = GPIO_MODE_INPUT;
    in_conf.pin_bit_mask = 1ULL << pins_.busy;
    in_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    in_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // BUSY idles LOW
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&in_conf));

    gpio_set_level(pins_.cs, 1);
    gpio_set_level(pins_.dc, 1);
    gpio_set_level(pins_.rst, 1);

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = -1;  // panel is write-only
    buscfg.mosi_io_num = pins_.mosi;
    buscfg.sclk_io_num = pins_.sck;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = kMaxTransferSize;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num = -1;  // CS is driven manually
    devcfg.clock_speed_hz = kSpiClockHz;
    devcfg.mode = 0;
    devcfg.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_initialize(pins_.spi_host, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(pins_.spi_host, &devcfg, &spi_));
}

void NmEpdDisplay::Transmit(const uint8_t* data, size_t len) {
    // Chunk through the internal-RAM DMA bounce buffer so callers can hand in
    // PSRAM-backed frame buffers.
    size_t offset = 0;
    while (offset < len) {
        const size_t chunk = std::min(kMaxTransferSize, len - offset);
        spi_transaction_t t = {};
        t.length = chunk * 8;
        if (chunk <= sizeof(t.tx_data)) {
            t.flags = SPI_TRANS_USE_TXDATA;
            memcpy(t.tx_data, data + offset, chunk);
        } else {
            memcpy(dma_buffer_, data + offset, chunk);
            t.tx_buffer = dma_buffer_;
        }
        ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_polling_transmit(spi_, &t));
        offset += chunk;
    }
}

void NmEpdDisplay::SendCommand(uint8_t command) {
    gpio_set_level(pins_.dc, 0);
    gpio_set_level(pins_.cs, 0);
    Transmit(&command, 1);
    gpio_set_level(pins_.cs, 1);
}

void NmEpdDisplay::SendData(uint8_t data) { SendBytes(&data, 1); }

void NmEpdDisplay::SendBytes(const uint8_t* data, size_t len) {
    gpio_set_level(pins_.dc, 1);
    gpio_set_level(pins_.cs, 0);
    Transmit(data, len);
    gpio_set_level(pins_.cs, 1);
}

void NmEpdDisplay::WaitBusy(const char* operation) {
    const int64_t start = esp_timer_get_time();
    // HIGH while the controller is refreshing / resetting.
    while (gpio_get_level(pins_.busy) == 1) {
        if (esp_timer_get_time() - start >= (int64_t)kBusyTimeoutMs * 1000) {
            ESP_LOGE(TAG, "BUSY timeout during %s", operation);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void NmEpdDisplay::HardwareReset() {
    gpio_set_level(pins_.rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(pins_.rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    WaitBusy("reset");
}

void NmEpdDisplay::SetRamArea(int x, int y, int w, int h) {
    SendCommand(0x11);  // RAM entry mode: X/Y increase
    SendData(0x03);
    SendCommand(0x44);  // RAM X start/end, in byte columns
    SendData((uint8_t)(x / 8));
    SendData((uint8_t)((x + w - 1) / 8));
    SendCommand(0x45);  // RAM Y start/end, in pixel rows
    SendData((uint8_t)(y & 0xFF));
    SendData((uint8_t)((y >> 8) & 0xFF));
    SendData((uint8_t)((y + h - 1) & 0xFF));
    SendData((uint8_t)(((y + h - 1) >> 8) & 0xFF));
    SendCommand(0x4E);  // RAM X address counter
    SendData((uint8_t)(x / 8));
    SendCommand(0x4F);  // RAM Y address counter
    SendData((uint8_t)(y & 0xFF));
    SendData((uint8_t)((y >> 8) & 0xFF));
}

void NmEpdDisplay::InitPanel() {
    HardwareReset();
    SendCommand(0x12);  // SWRESET
    WaitBusy("swreset");
    SendCommand(0x01);  // driver output control, MUX = height - 1
    SendData((uint8_t)((height_ - 1) & 0xFF));
    SendData((uint8_t)(((height_ - 1) >> 8) & 0xFF));
    SendData(0x00);
    SendCommand(0x3C);  // border waveform
    SendData(0x01);
    SendCommand(0x18);  // use built-in temperature sensor
    SendData(0x80);
    SetRamArea(0, 0, width_, height_);
}

void NmEpdDisplay::RefreshFull(const uint8_t* frame) {
    const int64_t start = esp_timer_get_time();

    // Load the same frame into both RAM banks: the controller's differential
    // waveform then sees no stale ghosting, and a valid baseline is left in
    // place for following partial refreshes.
    SetRamArea(0, 0, width_, height_);
    SendCommand(0x26);  // "previous image" RAM
    SendBytes(frame, frame_bytes_);
    SetRamArea(0, 0, width_, height_);
    SendCommand(0x24);  // "current image" RAM
    SendBytes(frame, frame_bytes_);

    SendCommand(0x21);  // display update control: bypass RED RAM, single chip
    SendData(0x40);
    SendData(0x00);
    SendCommand(0x1A);  // write temperature register (fast full waveform)
    SendData(0x64);
    SendCommand(0x22);
    SendData(0xD7);     // fast full update sequence
    SendCommand(0x20);  // master activation
    WaitBusy("full refresh");

    ESP_LOGI(TAG, "full refresh: %lld ms", (esp_timer_get_time() - start) / 1000);
}

void NmEpdDisplay::RefreshPartial(const NmEpdRect& rect, const uint8_t* frame) {
    const int64_t start = esp_timer_get_time();
    const int stride = width_ / 8;
    const int row_bytes = rect.w / 8;

    auto send_window = [&](uint8_t command) {
        SetRamArea(rect.x, rect.y, rect.w, rect.h);
        SendCommand(command);
        gpio_set_level(pins_.dc, 1);
        gpio_set_level(pins_.cs, 0);
        for (int row = 0; row < rect.h; row++) {
            Transmit(frame + (rect.y + row) * stride + rect.x / 8, row_bytes);
        }
        gpio_set_level(pins_.cs, 1);
    };

    send_window(0x24);  // new window into "current image" RAM
    SendCommand(0x21);
    SendData(0x00);
    SendData(0x00);
    SendCommand(0x22);
    SendData(0xFC);  // partial update sequence
    SendCommand(0x20);
    WaitBusy("partial refresh");

    // SSD1683 does not copy current RAM to previous RAM after a partial
    // update; mirror the window into both banks so the next differential
    // waveform has the displayed image as its baseline.
    send_window(0x24);
    send_window(0x26);

    ESP_LOGI(TAG, "partial refresh %dx%d@(%d,%d): %lld ms", rect.w, rect.h, rect.x, rect.y,
             (esp_timer_get_time() - start) / 1000);
}

void NmEpdDisplay::lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p) {
    assert(disp && area && color_p);
    NmEpdDisplay* driver = (NmEpdDisplay*)lv_display_get_user_data(disp);
    assert(driver);

    const uint16_t* src = (const uint16_t*)color_p;
    const int x1 = std::max(0, (int)area->x1);
    const int y1 = std::max(0, (int)area->y1);
    const int x2 = std::min(driver->width_ - 1, (int)area->x2);
    const int y2 = std::min(driver->height_ - 1, (int)area->y2);
    const int src_w = area->x2 - area->x1 + 1;
    const int stride = driver->width_ / 8;

    xSemaphoreTake(driver->dirty_mutex_, portMAX_DELAY);
    for (int y = y1; y <= y2; y++) {
        const uint16_t* row = src + (y - y1 + (y1 - area->y1)) * src_w + (x1 - area->x1);
        for (int x = x1; x <= x2; x++) {
            SetPixel1bpp(driver->frame_, stride, x, y, Rgb565IsWhite(row[x - x1], kBwThreshold));
        }
    }

    NmEpdRect r = AlignRectX8({x1, y1, x2 - x1 + 1, y2 - y1 + 1}, driver->width_, driver->height_);
    if (RectArea(r) > 0) {
        driver->dirty_ = RectUnion(driver->dirty_, r);
        driver->pending_ = true;
        driver->last_activity_tick_ = xTaskGetTickCount();
        if (driver->refresh_task_) {
            xTaskNotifyGive(driver->refresh_task_);
        }
    }
    xSemaphoreGive(driver->dirty_mutex_);

    lv_disp_flush_ready(disp);
}

void NmEpdDisplay::SetupUI() {
    LcdDisplay::SetupUI();
    DisplayLockGuard lock(this);
    if (bottom_bar_ == nullptr || chat_message_label_ == nullptr) {
        return;
    }
    // Reserve the lower ~50% of the screen for wrapped multi-line chat text
    // (CONFIG_USE_MULTILINE_CHAT_MESSAGE makes bottom_bar_ content-sized;
    // pin it to a fixed height so the chat area layout is stable).
    lv_obj_set_height(bottom_bar_, height_ / 2);
    lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    // Text grows downward from the top of the reserved area.
    lv_obj_align(chat_message_label_, LV_ALIGN_TOP_MID, 0, 0);
    // Fully opaque bars: LVGL then does not need to re-composite the layers
    // beneath them on every change, which keeps dirty areas small on EPD.
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_COVER, 0);
    // Move the emoji into the upper half so it never overlaps the chat area.
    if (emoji_box_ != nullptr) {
        lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, -height_ / 4);
    }
#if CONFIG_USE_MULTILINE_CHAT_MESSAGE
    ESP_LOGI(TAG, "multiline chat message area: bottom %d px", height_ / 2);
#else
    ESP_LOGW(TAG, "CONFIG_USE_MULTILINE_CHAT_MESSAGE is NOT enabled!");
#endif
}

void NmEpdDisplay::SetChatMessage(const char* role, const char* content) {
    // Mirrors LcdDisplay::SetChatMessage (multiline variant) without calling
    // the base implementation, then trims over-long text so it never grows
    // past the reserved chat area.
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_anim_delete(chat_message_label_, nullptr);
    lv_label_set_text(chat_message_label_, content);
    // Show bottom_bar_ only when there is content (and subtitle is not globally hidden)
    if (bottom_bar_ != nullptr) {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        } else if (!hide_subtitle_) {
            lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (bottom_bar_ == nullptr || content == nullptr || content[0] == '\0') {
        return;
    }
    // Keep only the tail of long messages: drop the oldest characters until
    // the wrapped label fits inside the reserved area.
    lv_obj_update_layout(bottom_bar_);
    const lv_coord_t avail = lv_obj_get_content_height(bottom_bar_);
    std::string text = content;
    while (text.size() > kChatTrimStep && lv_obj_get_height(chat_message_label_) > avail) {
        text.erase(0, kChatTrimStep);
        lv_label_set_text(chat_message_label_, text.c_str());
        lv_obj_update_layout(chat_message_label_);
    }
}

void NmEpdDisplay::SetStatus(const char* status) {
    // E-paper: the stock behavior mirrors the device state ("listening",
    // "speaking", "standby", ...) into the top status bar on every
    // transition, dirtying the screen several times per conversation turn.
    // Show only the wall clock instead, refreshed at most once per minute
    // (system time is synced from the OTA server response).
    (void)status;
    DisplayLockGuard lock(this);
    if (status_label_ == nullptr) {
        return;
    }
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    const bool time_valid = tm_now != nullptr && tm_now->tm_year >= 2025 - 1900;
    if (!time_valid) {
        // Keep whatever SetupUI() seeded (e.g. "initializing") until the
        // clock is synced.
        return;
    }
    const int cur_min = tm_now->tm_hour * 60 + tm_now->tm_min;
    if (cur_min == last_clock_min_) {
        return;
    }
    last_clock_min_ = cur_min;
    char time_str[8];
    strftime(time_str, sizeof(time_str), "%H:%M", tm_now);
    lv_label_set_text(status_label_, time_str);
    lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
}

void NmEpdDisplay::RequestFullRefresh() {
    if (!dirty_mutex_)
        return;
    xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
    force_full_refresh_ = true;
    pending_ = true;
    dirty_ = {0, 0, width_, height_};
    xSemaphoreGive(dirty_mutex_);
    if (refresh_task_) {
        xTaskNotifyGive(refresh_task_);
    }
}

bool NmEpdDisplay::IsRefreshPending() {
    if (!dirty_mutex_)
        return false;
    xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
    bool busy = pending_ || force_full_refresh_;
    xSemaphoreGive(dirty_mutex_);
    return busy;
}

void NmEpdDisplay::StartRefreshTask() {
    xTaskCreatePinnedToCore(RefreshTaskEntry, "epd_refresh", 4096, this, 3, &refresh_task_, 1);
}

void NmEpdDisplay::RefreshTaskEntry(void* arg) { ((NmEpdDisplay*)arg)->RefreshTaskLoop(); }

void NmEpdDisplay::RefreshTaskLoop() {
    uint32_t stat_full = 0, stat_partial = 0, stat_skip = 0;
    TickType_t stat_window_start = xTaskGetTickCount();

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        // Periodic 3 s refresh statistics so serial logs show exactly what
        // the panel is doing (full vs partial vs skipped).
        const TickType_t tick_now = xTaskGetTickCount();
        if ((stat_full | stat_partial | stat_skip) != 0 &&
            tick_now - stat_window_start >= pdMS_TO_TICKS(3000)) {
            ESP_LOGI(TAG, "refresh stats: full=%u partial=%u skip=%u (last ~3s)",
                     (unsigned)stat_full, (unsigned)stat_partial, (unsigned)stat_skip);
            stat_full = stat_partial = stat_skip = 0;
            stat_window_start = tick_now;
        }

        NmEpdRect rect = {0, 0, 0, 0};
        bool force_full = false;
        xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
        if (pending_ && RectArea(dirty_) > 0) {
            rect = dirty_;
            dirty_ = {0, 0, 0, 0};
            pending_ = false;
            force_full = force_full_refresh_;
            force_full_refresh_ = false;
        }
        xSemaphoreGive(dirty_mutex_);

        if (RectArea(rect) > 0 && !force_full) {
            // Streaming-aware debounce: while chat text streams in, LVGL
            // flushes many small updates per second. Hold the panel update
            // until the UI has been quiet for kQuietDebounceMs (or the hold
            // times out), merging all dirty areas into a single refresh.
            const TickType_t hold_start = xTaskGetTickCount();
            while (true) {
                const TickType_t now = xTaskGetTickCount();
                if (now - last_activity_tick_ >= pdMS_TO_TICKS(kQuietDebounceMs)) {
                    break;
                }
                if (now - hold_start >= pdMS_TO_TICKS(kMaxDebounceHoldMs)) {
                    break;
                }
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
                xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
                if (pending_ && RectArea(dirty_) > 0) {
                    rect = RectUnion(rect, dirty_);
                    dirty_ = {0, 0, 0, 0};
                    pending_ = false;
                    if (force_full_refresh_) {
                        force_full = true;
                        force_full_refresh_ = false;
                    }
                }
                xSemaphoreGive(dirty_mutex_);
                if (force_full) {
                    break;
                }
            }
        }

        if (RectArea(rect) > 0) {
            xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
            if (pending_ && RectArea(dirty_) > 0) {
                rect = RectUnion(rect, dirty_);
                dirty_ = {0, 0, 0, 0};
                pending_ = false;
                if (force_full_refresh_) {
                    force_full = true;
                    force_full_refresh_ = false;
                }
            }
            memcpy(tx_frame_, frame_, frame_bytes_);
            xSemaphoreGive(dirty_mutex_);
        }

        if (RectArea(rect) == 0) {
            // No UI update pending. If a periodic anti-ghosting full refresh
            // was deferred while text was streaming in, run it now that the
            // UI has been idle for a while.
            if (idle_full_due_ && prev_valid_) {
                const TickType_t now = xTaskGetTickCount();
                if (now - last_activity_tick_ >= pdMS_TO_TICKS(kIdleFullRefreshDelayMs)) {
                    idle_full_due_ = false;
                    xSemaphoreTake(dirty_mutex_, portMAX_DELAY);
                    memcpy(tx_frame_, frame_, frame_bytes_);
                    xSemaphoreGive(dirty_mutex_);
                    ESP_LOGI(TAG, "full refresh reason=idle-cleanup");
                    RefreshFull(tx_frame_);
                    memcpy(prev_frame_, tx_frame_, frame_bytes_);
                    partial_since_full_ = 0;
                    stat_full++;
                    last_refresh_tick_ = xTaskGetTickCount();
                }
            }
            continue;
        }

        // Throttle back-to-back partial refreshes; the panel needs waveform
        // time anyway and LVGL tends to flush chat text incrementally.
        const TickType_t now = xTaskGetTickCount();
        if (!force_full && last_refresh_tick_ != 0 &&
            now - last_refresh_tick_ < pdMS_TO_TICKS(kMinRefreshIntervalMs)) {
            vTaskDelay(pdMS_TO_TICKS(kMinRefreshIntervalMs) - (now - last_refresh_tick_));
        }

        // Measure how much actually changed against the panel's content.
        // Note: do NOT promote large dirty rects to full refreshes here.
        // A chat update redraws both the top status bar and the bottom
        // chat area, and their union covers the whole screen -- treating
        // that as a full refresh would blank the panel on every message.
        // The partial waveform handles large windows fine; genuine full
        // screen changes still trip the diff-ratio check below.
        size_t diff_bits = 0;
        for (int i = 0; i < frame_bytes_; i++) {
            diff_bits += __builtin_popcount((unsigned)(tx_frame_[i] ^ prev_frame_[i]));
        }
        if (diff_bits == 0 && prev_valid_) {
            stat_skip++;
            continue;
        }
        const float diff_ratio = (float)diff_bits / (float)(frame_bytes_ * 8);

        const char* full_reason = nullptr;
        if (force_full) {
            full_reason = "forced";
        } else if (!prev_valid_) {
            full_reason = "no-baseline";
        } else if (diff_ratio >= kForceFullDiffRatio) {
            full_reason = "diff-ratio";
        }
        const bool full = full_reason != nullptr;
        if (!full && partial_since_full_ >= kMaxPartialPerFull) {
            // The periodic anti-ghosting full refresh must not interrupt a
            // stream of UI updates (it blanks the panel for ~2.5 s); keep
            // doing partial updates and run it from the idle path instead.
            idle_full_due_ = true;
        }

        if (full) {
            ESP_LOGI(TAG, "full refresh reason=%s diff=%.1f%% rect=%dx%d@(%d,%d)", full_reason,
                     (double)(diff_ratio * 100.0f), rect.w, rect.h, rect.x, rect.y);
            RefreshFull(tx_frame_);
            memcpy(prev_frame_, tx_frame_, frame_bytes_);
            prev_valid_ = true;
            partial_since_full_ = 0;
            idle_full_due_ = false;
            stat_full++;
        } else {
            RefreshPartial(rect, tx_frame_);
            // Keep prev_frame_ in sync with the panel for the next diff.
            const int stride = width_ / 8;
            const int row_bytes = rect.w / 8;
            for (int row = 0; row < rect.h; row++) {
                memcpy(prev_frame_ + (rect.y + row) * stride + rect.x / 8,
                       tx_frame_ + (rect.y + row) * stride + rect.x / 8, row_bytes);
            }
            partial_since_full_++;
            stat_partial++;
        }
        last_refresh_tick_ = xTaskGetTickCount();
    }
}
