#include "t-deck-max_epd_display.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

namespace {

constexpr const char* kTag = "TDeckMaxEpdDisplay";
constexpr int kSpiMaxTransferBytes = 4096;
constexpr int kEpdBusyLevel = 0;
constexpr int64_t kBusyTimeoutUs = 10 * 1000 * 1000LL;

void* AllocateLvglBuffer(size_t size) {
    void* buffer = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == nullptr) {
        buffer = heap_caps_calloc(1, size, MALLOC_CAP_8BIT);
    }
    return buffer;
}

}  // namespace

TDeckMaxEpdDisplay::TDeckMaxEpdDisplay()
    : LcdDisplay(nullptr, nullptr, T_DECK_MAX_EPD_WIDTH, T_DECK_MAX_EPD_HEIGHT) {
    InitializeGpio();
    if (!InitializeSpi()) {
        return;
    }

    frame_buffer_ =
        static_cast<uint8_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    lvgl_buffer_ = static_cast<uint8_t*>(AllocateLvglBuffer(kLvglBufferBytes));
    if (frame_buffer_ == nullptr || lvgl_buffer_ == nullptr) {
        ESP_LOGE(kTag, "Failed to allocate EPD/LVGL buffers");
        return;
    }
    memset(frame_buffer_, 0xFF, kFrameBytes);

    ESP_LOGI(kTag, "Initialize LVGL");
    lv_init();
    lvgl_port_cfg_t port_config = ESP_LVGL_PORT_INIT_CONFIG();
    port_config.task_priority = 1;
    port_config.timer_period_ms = 50;
    lvgl_port_init(&port_config);

    if (!lvgl_port_lock(30000)) {
        ESP_LOGE(kTag, "Failed to lock LVGL during EPD setup");
        return;
    }
    display_ = lv_display_create(T_DECK_MAX_EPD_WIDTH, T_DECK_MAX_EPD_HEIGHT);
    if (display_ != nullptr) {
        lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
        lv_display_set_flush_cb(display_, LvglFlushCallback);
        lv_display_set_user_data(display_, this);
        lv_display_set_buffers(display_, lvgl_buffer_, nullptr, kLvglBufferBytes,
                               LV_DISPLAY_RENDER_MODE_FULL);
    }
    lvgl_port_unlock();

    if (display_ == nullptr) {
        ESP_LOGE(kTag, "Failed to create LVGL EPD display");
        return;
    }

    panel_ready_ = InitializePanel();
    ESP_LOGI(kTag, "UC8253 EPD %s", panel_ready_ ? "ready" : "initialization failed");
}

void TDeckMaxEpdDisplay::InitializeGpio() {
    gpio_config_t output_config = {};
    output_config.pin_bit_mask = (1ULL << T_DECK_MAX_EPD_CS) | (1ULL << T_DECK_MAX_EPD_DC) |
                                 (1ULL << T_DECK_MAX_EPD_RST) | (1ULL << T_DECK_MAX_EPD_BL) |
                                 (1ULL << T_DECK_MAX_SD_CS);
    output_config.mode = GPIO_MODE_OUTPUT;
    output_config.pull_up_en = GPIO_PULLUP_DISABLE;
    output_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t ret = gpio_config(&output_config);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "EPD output GPIO setup failed: %s", esp_err_to_name(ret));
    }

    gpio_config_t busy_config = {};
    busy_config.pin_bit_mask = 1ULL << T_DECK_MAX_EPD_BUSY;
    busy_config.mode = GPIO_MODE_INPUT;
    busy_config.pull_up_en = GPIO_PULLUP_DISABLE;
    busy_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    busy_config.intr_type = GPIO_INTR_DISABLE;
    ret = gpio_config(&busy_config);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "EPD BUSY GPIO setup failed: %s", esp_err_to_name(ret));
    }

    // Keep the shared SPI devices deselected while the EPD is initialized.
    gpio_set_level(T_DECK_MAX_SD_CS, 1);
    gpio_set_level(T_DECK_MAX_EPD_CS, 1);
    gpio_set_level(T_DECK_MAX_EPD_DC, 1);
    gpio_set_level(T_DECK_MAX_EPD_RST, 1);
    gpio_set_level(T_DECK_MAX_EPD_BL, 1);
}

bool TDeckMaxEpdDisplay::InitializeSpi() {
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = T_DECK_MAX_EPD_MOSI;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = T_DECK_MAX_EPD_SCK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = kSpiMaxTransferBytes;

    esp_err_t ret = spi_bus_initialize(T_DECK_MAX_EPD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "EPD SPI bus setup failed: %s", esp_err_to_name(ret));
        return false;
    }

    spi_device_interface_config_t device_config = {};
    device_config.clock_speed_hz = 2 * 1000 * 1000;
    device_config.mode = 0;
    device_config.spics_io_num = -1;
    device_config.queue_size = 1;
    ret = spi_bus_add_device(T_DECK_MAX_EPD_SPI_HOST, &device_config, &spi_);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "EPD SPI device setup failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

void TDeckMaxEpdDisplay::HardwareReset() {
    gpio_set_level(T_DECK_MAX_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(T_DECK_MAX_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(T_DECK_MAX_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    panel_registers_initialized_ = false;
    power_is_on_ = false;
}

bool TDeckMaxEpdDisplay::WaitWhileBusy(const char* operation) {
    const int64_t deadline = esp_timer_get_time() + kBusyTimeoutUs;
    vTaskDelay(pdMS_TO_TICKS(1));
    while (gpio_get_level(T_DECK_MAX_EPD_BUSY) == kEpdBusyLevel) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGE(kTag, "EPD BUSY timeout during %s", operation);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return true;
}

void TDeckMaxEpdDisplay::SendCommand(uint8_t command) {
    if (spi_ == nullptr) {
        return;
    }
    spi_transaction_t transaction = {};
    transaction.length = 8;
    transaction.tx_buffer = &command;
    gpio_set_level(T_DECK_MAX_EPD_DC, 0);
    gpio_set_level(T_DECK_MAX_EPD_CS, 0);
    const esp_err_t ret = spi_device_polling_transmit(spi_, &transaction);
    gpio_set_level(T_DECK_MAX_EPD_CS, 1);
    gpio_set_level(T_DECK_MAX_EPD_DC, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "EPD command 0x%02X failed: %s", command, esp_err_to_name(ret));
    }
}

void TDeckMaxEpdDisplay::SendData(uint8_t data) {
    if (spi_ == nullptr) {
        return;
    }
    spi_transaction_t transaction = {};
    transaction.length = 8;
    transaction.tx_buffer = &data;
    gpio_set_level(T_DECK_MAX_EPD_DC, 1);
    gpio_set_level(T_DECK_MAX_EPD_CS, 0);
    const esp_err_t ret = spi_device_polling_transmit(spi_, &transaction);
    gpio_set_level(T_DECK_MAX_EPD_CS, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "EPD data write failed: %s", esp_err_to_name(ret));
    }
}

bool TDeckMaxEpdDisplay::SendBytes(const uint8_t* data, size_t size) {
    if (spi_ == nullptr || data == nullptr) {
        return false;
    }
    if (size == 0) {
        return true;
    }

    gpio_set_level(T_DECK_MAX_EPD_DC, 1);
    gpio_set_level(T_DECK_MAX_EPD_CS, 0);
    size_t offset = 0;
    while (offset < size) {
        const size_t chunk = std::min(size - offset, static_cast<size_t>(kSpiMaxTransferBytes));
        spi_transaction_t transaction = {};
        transaction.length = chunk * 8;
        transaction.tx_buffer = data + offset;
        const esp_err_t ret = spi_device_polling_transmit(spi_, &transaction);
        if (ret != ESP_OK) {
            gpio_set_level(T_DECK_MAX_EPD_CS, 1);
            ESP_LOGE(kTag, "EPD frame write failed at %u: %s", static_cast<unsigned>(offset),
                     esp_err_to_name(ret));
            return false;
        }
        offset += chunk;
    }
    gpio_set_level(T_DECK_MAX_EPD_CS, 1);
    return true;
}

bool TDeckMaxEpdDisplay::NormalizeWindow(const lv_area_t* area, EpdWindow* window) const {
    if (area == nullptr || window == nullptr || area->x1 > area->x2 || area->y1 > area->y2) {
        return false;
    }

    const int x1 = std::max(0, static_cast<int>(area->x1));
    const int y1 = std::max(0, static_cast<int>(area->y1));
    const int x2 = std::min(T_DECK_MAX_EPD_WIDTH - 1, static_cast<int>(area->x2));
    const int y2 = std::min(T_DECK_MAX_EPD_HEIGHT - 1, static_cast<int>(area->y2));
    if (x1 > x2 || y1 > y2) {
        return false;
    }

    const uint16_t aligned_x = static_cast<uint16_t>(x1 & ~0x0007);
    const uint16_t aligned_x_end =
        static_cast<uint16_t>(std::min(T_DECK_MAX_EPD_WIDTH - 1, x2 | 0x0007));
    window->x = aligned_x;
    window->y = static_cast<uint16_t>(y1);
    window->width = aligned_x_end - aligned_x + 1;
    window->height = static_cast<uint16_t>(y2 - y1 + 1);
    return true;
}

void TDeckMaxEpdDisplay::SetPartialRamArea(const EpdWindow& window) {
    const uint16_t x_start = window.x & 0xFFF8;
    const uint16_t x_end = (window.x + window.width - 1) | 0x0007;
    const uint16_t y_start = window.y;
    const uint16_t y_end = window.y + window.height - 1;

    // UC8253 0x90 takes one byte for each horizontal endpoint and two for Y.
    SendCommand(0x90);
    SendData(static_cast<uint8_t>(x_start));
    SendData(static_cast<uint8_t>(x_end));
    SendData(static_cast<uint8_t>(y_start >> 8));
    SendData(static_cast<uint8_t>(y_start & 0xFF));
    SendData(static_cast<uint8_t>(y_end >> 8));
    SendData(static_cast<uint8_t>(y_end & 0xFF));
    SendData(0x01);
}

void TDeckMaxEpdDisplay::InitializePanelRegisters() {
    if (panel_registers_initialized_) {
        return;
    }

    // This is GxEPD2_310_GDEQ031T10::_InitDisplay(), for UC8253.
    SendCommand(0x00);
    SendData(0x1E);
    SendData(0x0D);
    vTaskDelay(pdMS_TO_TICKS(1));
    SendCommand(0x00);
    SendData(0x1F);
    SendData(0x0D);
    panel_registers_initialized_ = true;
}

bool TDeckMaxEpdDisplay::WriteFullFrameBuffer(uint8_t command) {
    if (frame_buffer_ == nullptr || spi_ == nullptr) {
        return false;
    }
    InitializePanelRegisters();
    SendCommand(command);
    return SendBytes(frame_buffer_, kFrameBytes);
}

bool TDeckMaxEpdDisplay::WriteFrameRegion(uint8_t command, const EpdWindow& window) {
    if (frame_buffer_ == nullptr || spi_ == nullptr || window.width == 0 || window.height == 0 ||
        (window.width % 8) != 0) {
        return false;
    }

    InitializePanelRegisters();
    SendCommand(0x91);  // partial RAM area in
    SetPartialRamArea(window);
    SendCommand(command);

    const size_t row_bytes = window.width / 8;
    gpio_set_level(T_DECK_MAX_EPD_DC, 1);
    gpio_set_level(T_DECK_MAX_EPD_CS, 0);
    bool sent = true;
    for (uint16_t row = 0; row < window.height; ++row) {
        const uint8_t* row_data =
            frame_buffer_ + (static_cast<size_t>(window.y + row) * (T_DECK_MAX_EPD_WIDTH / 8)) +
            (window.x / 8);
        spi_transaction_t transaction = {};
        transaction.length = row_bytes * 8;
        transaction.tx_buffer = row_data;
        const esp_err_t ret = spi_device_polling_transmit(spi_, &transaction);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "EPD region write failed at row %u: %s", static_cast<unsigned>(row),
                     esp_err_to_name(ret));
            sent = false;
            break;
        }
    }
    gpio_set_level(T_DECK_MAX_EPD_CS, 1);
    SendCommand(0x92);  // partial RAM area out
    return sent;
}

bool TDeckMaxEpdDisplay::PowerOn() {
    if (power_is_on_) {
        return true;
    }
    SendCommand(0x04);
    power_is_on_ = WaitWhileBusy("power on");
    return power_is_on_;
}

bool TDeckMaxEpdDisplay::PowerOff() {
    if (!power_is_on_) {
        return true;
    }
    SendCommand(0x02);
    const bool ready = WaitWhileBusy("power off");
    power_is_on_ = false;
    return ready;
}

bool TDeckMaxEpdDisplay::Refresh(const EpdWindow& window, bool full_refresh) {
    if (!full_refresh) {
        SendCommand(0x91);  // partial RAM area in
        SetPartialRamArea(window);
    }

    if (full_refresh) {
        SendCommand(0xE0);  // Cascade Setting (CCSET)
        SendData(0x02);     // TSFIX
        SendCommand(0xE5);  // Force Temperature (TSSET)
        SendData(0x5A);     // 90 C, as in GxEPD2
    } else {
        SendCommand(0xE0);
        SendData(0x02);
        SendCommand(0xE5);
        SendData(0x79);  // 121 C, fast partial waveform
    }
    SendCommand(0x50);
    SendData(full_refresh ? 0x97 : 0xD7);

    if (!PowerOn()) {
        if (!full_refresh) {
            SendCommand(0x92);
        }
        panel_registers_initialized_ = false;
        return false;
    }

    SendCommand(0x12);  // display refresh
    const bool ready = WaitWhileBusy(full_refresh ? "full refresh" : "partial refresh");
    if (!full_refresh) {
        SendCommand(0x92);  // partial RAM area out
    }
    // GxEPD2 deliberately reinitializes UC8253 after each refresh.
    panel_registers_initialized_ = false;
    return ready;
}

bool TDeckMaxEpdDisplay::InitializePanel() {
    HardwareReset();
    memset(frame_buffer_, 0xFF, kFrameBytes);

    const EpdWindow full_window = {
        0,
        0,
        T_DECK_MAX_EPD_WIDTH,
        T_DECK_MAX_EPD_HEIGHT,
    };
    if (!WriteFullFrameBuffer(0x10) || !WriteFullFrameBuffer(0x13) || !Refresh(full_window, true) ||
        !PowerOff()) {
        return false;
    }
    first_content_refresh_ = true;
    return true;
}

void TDeckMaxEpdDisplay::UpdateFrameBuffer(const lv_area_t* area, const uint8_t* color_buffer) {
    if (area == nullptr || color_buffer == nullptr || frame_buffer_ == nullptr ||
        area->x1 > area->x2 || area->y1 > area->y2) {
        return;
    }

    const int area_width = lv_area_get_width(area);
    if (area_width <= 0) {
        return;
    }
    const uint16_t* pixels = reinterpret_cast<const uint16_t*>(color_buffer);
    const int x_start = std::max(0, static_cast<int>(area->x1));
    const int y_start = std::max(0, static_cast<int>(area->y1));
    const int x_end = std::min(T_DECK_MAX_EPD_WIDTH - 1, static_cast<int>(area->x2));
    const int y_end = std::min(T_DECK_MAX_EPD_HEIGHT - 1, static_cast<int>(area->y2));
    if (x_start > x_end || y_start > y_end) {
        return;
    }

    const size_t stride = T_DECK_MAX_EPD_WIDTH / 8;
    for (int y = y_start; y <= y_end; ++y) {
        for (int x = x_start; x <= x_end; ++x) {
            const int source_x = x - area->x1;
            const int source_y = y - area->y1;
            const size_t source_index =
                static_cast<size_t>(source_y) * static_cast<size_t>(area_width) +
                static_cast<size_t>(source_x);
            const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 0x07));
            uint8_t& byte =
                frame_buffer_[static_cast<size_t>(y) * stride + static_cast<size_t>(x >> 3)];
            const uint16_t pixel = pixels[source_index];
            const uint8_t red = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
            const uint8_t green = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
            const uint8_t blue = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
            const uint16_t brightness =
                static_cast<uint16_t>((red * 299 + green * 587 + blue * 114) / 1000);
            if (brightness < 128) {
                byte &= static_cast<uint8_t>(~mask);
            } else {
                byte |= mask;
            }
        }
    }
}

void TDeckMaxEpdDisplay::LvglFlushCallback(lv_display_t* display, const lv_area_t* area,
                                           uint8_t* color_buffer) {
    auto* driver = static_cast<TDeckMaxEpdDisplay*>(lv_display_get_user_data(display));
    if (driver == nullptr) {
        lv_display_flush_ready(display);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(driver->epd_mutex_);
        TDeckMaxEpdDisplay::EpdWindow window{};
        if (driver->panel_ready_ && driver->NormalizeWindow(area, &window)) {
            driver->UpdateFrameBuffer(area, color_buffer);

            bool updated = false;
            if (driver->first_content_refresh_) {
                const EpdWindow full_window = {
                    0,
                    0,
                    T_DECK_MAX_EPD_WIDTH,
                    T_DECK_MAX_EPD_HEIGHT,
                };
                updated = driver->WriteFrameRegion(0x10, full_window) &&
                          driver->WriteFrameRegion(0x13, full_window) &&
                          driver->Refresh(full_window, true);
                if (updated) {
                    updated = driver->WriteFrameRegion(0x10, full_window);
                }
                if (updated) {
                    driver->first_content_refresh_ = false;
                }
            } else {
                updated = driver->WriteFrameRegion(0x13, window) && driver->Refresh(window, false);
                if (updated) {
                    updated = driver->WriteFrameRegion(0x10, window);
                }
            }

            const bool powered_off = driver->PowerOff();
            if (!updated || !powered_off) {
                ESP_LOGE(kTag, "EPD refresh failed for area (%d,%d)-(%d,%d)", area->x1, area->y1,
                         area->x2, area->y2);
            }
            ESP_LOGD(kTag, "LVGL flush area=(%d,%d)-(%d,%d), window=(%u,%u)+(%u,%u)", area->x1,
                     area->y1, area->x2, area->y2, window.x, window.y, window.width, window.height);
        }
    }
    lv_display_flush_ready(display);
}
