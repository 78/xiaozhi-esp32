#include "xz_epaper_display.h"

#ifdef EPAPER_USE_SSD1680
#include "epaper_waveforms.h"
#endif

#include <driver/spi_master.h>
#include <esp_check.h>
#include <esp_lcd_io_spi.h>
#include <esp_lvgl_port.h>
#include <esp_rom_sys.h>
#include <lvgl_private.h>

#include <algorithm>
#include <cstring>
#include <string_view>

namespace {
constexpr char kTag[] = "OlighkerEpaper";
constexpr int kBusyLevel = 1;
#ifdef EPAPER_USE_SSD1683
constexpr TickType_t kBusyTimeout = pdMS_TO_TICKS(10000);
constexpr TickType_t kPanelSleepDelay = pdMS_TO_TICKS(200);
#else
constexpr TickType_t kBusyTimeout = pdMS_TO_TICKS(5000);
constexpr TickType_t kPanelSleepDelay = pdMS_TO_TICKS(100);
#endif
constexpr TickType_t kCoalesceDelay = pdMS_TO_TICKS(100);
constexpr TickType_t kPanelIdleDelay = pdMS_TO_TICKS(8000);
constexpr unsigned kMaxPartialUpdates = 5;
constexpr int kSubtitleMargin = 6;
constexpr int kSubtitleWidth = DISPLAY_WIDTH - 2 * kSubtitleMargin;
constexpr int kSubtitleLines = DISPLAY_HEIGHT >= 200 ? 5 : 4;
constexpr uint32_t kSubtitlePageMs = 5000;

#ifdef EPAPER_USE_SSD1680
constexpr char kController[] = "SSD1680";
constexpr bool kRamRowsDescending = false;
constexpr uint8_t kGateDirection = 0x00;
#elif defined(EPAPER_USE_SSD1683)
constexpr char kController[] = "SSD1683";
constexpr bool kRamRowsDescending = false;
#else
constexpr char kController[] = "SSD1681";
constexpr bool kRamRowsDescending = true;
constexpr uint8_t kGateDirection = 0x01;

// See THIRD_PARTY_NOTICES.md for the waveform data license.
// The final six bytes configure the waveform end, drive voltages and VCOM.
constexpr uint8_t kSsd1681PartialWaveform[] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x02, 0x17, 0x41, 0xB0, 0x32, 0x28,
};
static_assert(sizeof(kSsd1681PartialWaveform) == 159);
#endif

constexpr uint8_t LowByte(uint16_t value) { return static_cast<uint8_t>(value); }

constexpr uint8_t HighByte(uint16_t value) { return static_cast<uint8_t>(value >> 8); }

std::string NormalizeSubtitle(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '\r' && index + 1 < text.size() && text[index + 1] == '\n') {
            continue;
        }
        result += ch == '\r' ? '\n' : ch;
    }
    return result;
}
}  // namespace

XzEpaperDisplay::XzEpaperDisplay() : LcdDisplay(nullptr, nullptr, DISPLAY_WIDTH, DISPLAY_HEIGHT) {}

XzEpaperDisplay::~XzEpaperDisplay() {
    if (subtitle_timer_ != nullptr) {
        DisplayLockGuard lock(this);
        lv_timer_delete(subtitle_timer_);
    }
    if (refresh_task_ != nullptr) {
        vTaskDelete(refresh_task_);
    }
    if (io_ != nullptr) {
        esp_lcd_panel_io_del(io_);
    }
    if (bus_initialized_) {
        spi_bus_free(EPAPER_SPI_HOST);
    }
}

bool XzEpaperDisplay::Initialize() {
    gpio_config_t pins = {};
    pins.pin_bit_mask = 1ULL << EPAPER_RESET_GPIO;
    pins.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&pins) != ESP_OK) {
        return false;
    }
    gpio_set_level(EPAPER_RESET_GPIO, 1);
    pins.pin_bit_mask = 1ULL << EPAPER_BUSY_GPIO;
    pins.mode = GPIO_MODE_INPUT;
    pins.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&pins) != ESP_OK) {
        return false;
    }

    spi_bus_config_t bus = {};
    bus.sclk_io_num = EPAPER_SCK_GPIO;
    bus.mosi_io_num = EPAPER_MOSI_GPIO;
    bus.miso_io_num = -1;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = spi_pixels_.size();
    if (spi_bus_initialize(EPAPER_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }
    bus_initialized_ = true;

    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num = EPAPER_CS_GPIO;
    io.dc_gpio_num = EPAPER_DC_GPIO;
    io.pclk_hz = 4000000;
    io.lcd_cmd_bits = 8;
    io.lcd_param_bits = 8;
    io.trans_queue_depth = 1;
    if (esp_lcd_new_panel_io_spi(EPAPER_SPI_HOST, &io, &io_) != ESP_OK) {
        return false;
    }

    const lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&port) != ESP_OK) {
        return false;
    }
    lvgl_port_lock(0);
    display_ = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (display_ == nullptr ||
        xTaskCreate([](void* context) { static_cast<XzEpaperDisplay*>(context)->Run(); },
                    "epaper_refresh", 4096, this, 1, &refresh_task_) != pdPASS) {
        if (display_ != nullptr) {
            lv_display_delete(display_);
            display_ = nullptr;
        }
        lvgl_port_unlock();
        return false;
    }
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display_, draw_buffer_.data(), nullptr, draw_buffer_.size(),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, Flush);
    lvgl_port_unlock();
    ESP_LOGI(kTag, "%s %dx%d interface ready", kController, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return true;
}

void XzEpaperDisplay::SetupUI() {
    LcdDisplay::SetupUI();
    DisplayLockGuard lock(this);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_MODE_WRAP);
    if (chat_message_label_ == nullptr || subtitle_timer_ != nullptr) {
        return;
    }

    lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(bottom_bar_, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bottom_bar_, kSubtitleMargin, 0);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_width(chat_message_label_, kSubtitleWidth);
    lv_obj_set_height(chat_message_label_, LV_SIZE_CONTENT);
    lv_obj_align(chat_message_label_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_MODE_WRAP);

    subtitle_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto* driver = static_cast<XzEpaperDisplay*>(lv_timer_get_user_data(timer));
            if (driver->subtitle_offset_ < driver->subtitle_.size()) {
                driver->ShowSubtitlePage();
                return;
            }
            lv_timer_pause(timer);
            if (driver->clear_after_pages_) {
                driver->ResetSubtitle();
            }
        },
        kSubtitlePageMs, this);
    if (subtitle_timer_ != nullptr) {
        lv_timer_pause(subtitle_timer_);
    }
}

void XzEpaperDisplay::ShowSubtitlePage() {
    const auto* font = lv_obj_get_style_text_font(chat_message_label_, LV_PART_MAIN);
    lv_text_attributes_t layout;
    lv_text_attributes_init(&layout);
    layout.max_width = kSubtitleWidth;
    layout.letter_space = lv_obj_get_style_text_letter_space(chat_message_label_, LV_PART_MAIN);

    const size_t start = subtitle_offset_;
    for (int line = 0; line < kSubtitleLines && subtitle_offset_ < subtitle_.size(); ++line) {
        const char* text = subtitle_.c_str() + subtitle_offset_;
        subtitle_offset_ += lv_text_get_next_line(text, subtitle_.size() - subtitle_offset_, font,
                                                  nullptr, &layout);
    }
    size_t end = subtitle_offset_;
    // A hard break at the end of a page must not create an extra empty line.
    if (end > start && subtitle_[end - 1] == '\n') {
        --end;
    }
    lv_label_set_text(chat_message_label_, subtitle_.substr(start, end - start).c_str());
}

void XzEpaperDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (subtitle_timer_ == nullptr) {
        LcdDisplay::SetChatMessage(role, content);
        return;
    }
    ResetSubtitle();
    if (content == nullptr || content[0] == '\0') {
        return;
    }

    assistant_subtitle_ = role != nullptr && std::string_view(role) == "assistant";
    subtitle_ = NormalizeSubtitle(content);
    if (!hide_subtitle_) {
        lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    ShowSubtitlePage();
    if (!hide_subtitle_ && subtitle_offset_ < subtitle_.size()) {
        lv_timer_reset(subtitle_timer_);
        lv_timer_resume(subtitle_timer_);
    }
}

void XzEpaperDisplay::ResetSubtitle() {
    lv_timer_pause(subtitle_timer_);
    subtitle_.clear();
    subtitle_offset_ = 0;
    assistant_subtitle_ = false;
    clear_after_pages_ = false;
    lv_label_set_text(chat_message_label_, "");
    lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
}

void XzEpaperDisplay::RemoveChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (subtitle_timer_ == nullptr) {
        LcdDisplay::RemoveChatMessage(role, content);
    } else if (content != nullptr &&
               (subtitle_ == content || subtitle_ == NormalizeSubtitle(content))) {
        ResetSubtitle();
    }
}

void XzEpaperDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (subtitle_timer_ == nullptr) {
        LcdDisplay::ClearChatMessages();
    } else if (assistant_subtitle_ && !lv_timer_get_paused(subtitle_timer_)) {
        // An idle transition can arrive before the reader has seen the last page.
        clear_after_pages_ = true;
    } else {
        ResetSubtitle();
    }
}

void XzEpaperDisplay::SetHideSubtitle(bool hide) {
    DisplayLockGuard lock(this);
    LcdDisplay::SetHideSubtitle(hide);
    if (subtitle_timer_ == nullptr) {
        return;
    }
    if (hide) {
        lv_timer_pause(subtitle_timer_);
    } else if (subtitle_offset_ < subtitle_.size() || clear_after_pages_) {
        lv_timer_reset(subtitle_timer_);
        lv_timer_resume(subtitle_timer_);
    }
}

void XzEpaperDisplay::Flush(lv_display_t* display, const lv_area_t*, uint8_t* pixels) {
    auto* driver = static_cast<XzEpaperDisplay*>(lv_display_get_user_data(display));
    pixels += kPaletteBytes;  // LVGL reserves two palette entries before the I1 pixels.
    bool changed = false;
    {
        const std::lock_guard lock(driver->frame_mutex_);
        changed = !driver->pending_valid_;
        for (size_t row = 0; row < EPAPER_RAM_HEIGHT; ++row) {
            auto* dest = driver->pending_.data() + row * kRowBytes;
#ifdef EPAPER_ROTATE_270
            // Logical (x, y) maps to controller (y, width - 1 - x).
            std::array<uint8_t, kRowBytes> rotated_row{};
            const size_t logical_x = DISPLAY_WIDTH - 1 - row;
            for (size_t column = 0; column < EPAPER_RAM_WIDTH; ++column) {
                const auto bit =
                    (pixels[column * kStride + logical_x / 8] >> (7 - logical_x % 8)) & 1;
                rotated_row[column / 8] |= bit << (7 - column % 8);
            }
            const auto* source = rotated_row.data();
#else
            const auto* source = pixels + row * kStride;
#endif
            if (std::memcmp(dest, source, kRowBytes) != 0) {
                std::memcpy(dest, source, kRowBytes);
                changed = true;
            }
        }
        driver->pending_valid_ = true;
    }
    lv_display_flush_ready(display);
    if (changed) {
        xTaskNotifyGive(driver->refresh_task_);
    }
}

esp_err_t XzEpaperDisplay::Command(uint8_t command, std::initializer_list<uint8_t> data) {
    return esp_lcd_panel_io_tx_param(io_, command, data.begin(), data.size());
}

esp_err_t XzEpaperDisplay::WaitIdle() {
    // Give the controller time to assert BUSY after a reset or update command.
    vTaskDelay(pdMS_TO_TICKS(20));
    const TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(EPAPER_BUSY_GPIO) == kBusyLevel) {
        if (xTaskGetTickCount() - start >= kBusyTimeout) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#ifdef EPAPER_USE_SSD1680
    // Let the panel settle after BUSY drops.
    vTaskDelay(pdMS_TO_TICKS(50));
#endif
    return ESP_OK;
}

esp_err_t XzEpaperDisplay::ResetPanel() {
    gpio_set_level(EPAPER_RESET_GPIO, 0);
    esp_rom_delay_us(2000);
    gpio_set_level(EPAPER_RESET_GPIO, 1);
#if defined(EPAPER_USE_SSD1680) || defined(EPAPER_USE_SSD1683)
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
    ESP_RETURN_ON_ERROR(WaitIdle(), kTag, "hardware reset");
    ESP_RETURN_ON_ERROR(Command(0x12), kTag, "software reset");
    ESP_RETURN_ON_ERROR(WaitIdle(), kTag, "software reset BUSY");
#ifndef EPAPER_USE_SSD1683
    constexpr uint16_t last_row = EPAPER_RAM_HEIGHT - 1;
    ESP_RETURN_ON_ERROR((Command(0x01, {LowByte(last_row), HighByte(last_row), kGateDirection})),
                        kTag, "gate output");
#endif
    ESP_RETURN_ON_ERROR(Command(0x11, {static_cast<uint8_t>(kRamRowsDescending ? 0x01 : 0x03)}),
                        kTag, "RAM entry mode");
#ifdef EPAPER_USE_SSD1680
    return Command(0x21, {0x00, 0x80});
#elif defined(EPAPER_USE_SSD1683)
    // Use the panel's native gate geometry and full-update RAM mode.
    ESP_RETURN_ON_ERROR((Command(0x21, {0x40, 0x00})), kTag, "SSD1683 full image control");
    return Command(0x3C, {0x05});
#else
    ESP_RETURN_ON_ERROR(Command(0x3C, {0x01}), kTag, "border waveform");
    return Command(0x18, {0x80});  // Use the controller's internal temperature sensor.
#endif
}

esp_err_t XzEpaperDisplay::LoadWaveform(const uint8_t* lut) {
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_, 0x32, lut, 153), kTag,
                        "write panel waveform");
    ESP_RETURN_ON_ERROR(WaitIdle(), kTag, "waveform BUSY");
    ESP_RETURN_ON_ERROR(Command(0x3F, {lut[153]}), kTag, "waveform end");
    ESP_RETURN_ON_ERROR(Command(0x03, {lut[154]}), kTag, "gate voltage");
    ESP_RETURN_ON_ERROR((Command(0x04, {lut[155], lut[156], lut[157]})), kTag, "source voltages");
    return Command(0x2C, {lut[158]});
}

esp_err_t XzEpaperDisplay::PreparePartial() {
    ESP_RETURN_ON_ERROR(ResetPanel(), kTag, "reset for partial mode");
#ifdef EPAPER_USE_SSD1680
    ESP_RETURN_ON_ERROR(LoadWaveform(kSsd1680DifferentialWaveform), kTag,
                        "SSD1680 differential waveform");
    // Use mode 2 for all waveform settings. Old/new RAM are managed by the driver.
    ESP_RETURN_ON_ERROR((Command(0x37, {0, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0, 0, 0, 0})), kTag,
                        "SSD1680 differential display options");
    return Command(0x3C, {0x80});
#else
#ifdef EPAPER_USE_SSD1683
    // Enable both RAM planes for the controller's OTP differential mode.
    ESP_RETURN_ON_ERROR((Command(0x21, {0x00, 0x00})), kTag, "SSD1683 partial image control");
#else
    ESP_RETURN_ON_ERROR(LoadWaveform(kSsd1681PartialWaveform), kTag, "SSD1681 partial waveform");
    ESP_RETURN_ON_ERROR((Command(0x37, {0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0})), kTag,
                        "partial display options");
#endif
    ESP_RETURN_ON_ERROR(Command(0x3C, {0x80}), kTag, "partial border waveform");
    // Recreate the baseline in RAM after reset, including after wake from deep sleep.
    return WriteBothImages(kFullScreen, displayed_);
#endif
}

esp_err_t XzEpaperDisplay::ActivateUpdate(uint8_t mode) {
    ESP_RETURN_ON_ERROR(Command(0x22, {mode}), kTag, "select update waveform");
    ESP_RETURN_ON_ERROR(Command(0x20), kTag, "start update");
    return WaitIdle();
}

XzEpaperDisplay::Region XzEpaperDisplay::FindChangedRegion() const {
    Region region{static_cast<int>(kRowBytes), -1, EPAPER_RAM_HEIGHT, -1};
    for (int row = 0; row < EPAPER_RAM_HEIGHT; ++row) {
        for (int column = 0; column < static_cast<int>(kRowBytes); ++column) {
            const size_t offset = row * kRowBytes + column;
            if (frame_[offset] != displayed_[offset]) {
                region.top = std::min(region.top, row);
                region.bottom = row;
                region.left_byte = std::min(region.left_byte, column);
                region.right_byte = std::max(region.right_byte, column);
            }
        }
    }
    return region;
}

esp_err_t XzEpaperDisplay::SetWindow(const Region& region) {
    const auto left = static_cast<uint8_t>(region.left_byte);
    const auto right = static_cast<uint8_t>(region.right_byte);
    const uint16_t first_y = kRamRowsDescending ? EPAPER_RAM_HEIGHT - 1 - region.top : region.top;
    const uint16_t last_y =
        kRamRowsDescending ? EPAPER_RAM_HEIGHT - 1 - region.bottom : region.bottom;
    ESP_RETURN_ON_ERROR((Command(0x44, {left, right})), kTag, "RAM columns");
    ESP_RETURN_ON_ERROR(
        (Command(0x45, {LowByte(first_y), HighByte(first_y), LowByte(last_y), HighByte(last_y)})),
        kTag, "RAM rows");
    ESP_RETURN_ON_ERROR(Command(0x4E, {left}), kTag, "column address");
    return Command(0x4F, {LowByte(first_y), HighByte(first_y)});
}

esp_err_t XzEpaperDisplay::WriteRegion(uint8_t command, const Region& region, const Frame& pixels) {
    ESP_RETURN_ON_ERROR(SetWindow(region), kTag, "set RAM window");
    const size_t width_bytes = region.right_byte - region.left_byte + 1;
    size_t length = 0;
    int next_command = command;
    for (int row = region.top; row <= region.bottom; ++row) {
        if (length + width_bytes > spi_pixels_.size()) {
            ESP_RETURN_ON_ERROR(
                esp_lcd_panel_io_tx_param(io_, next_command, spi_pixels_.data(), length), kTag,
                "write image RAM chunk");
            // The controller keeps advancing its RAM address across SPI transactions.
            next_command = -1;
            length = 0;
        }
        std::memcpy(spi_pixels_.data() + length, pixels.data() + row * kRowBytes + region.left_byte,
                    width_bytes);
        length += width_bytes;
    }
    return esp_lcd_panel_io_tx_param(io_, next_command, spi_pixels_.data(), length);
}

esp_err_t XzEpaperDisplay::WriteBothImages(const Region& region, const Frame& pixels) {
    ESP_RETURN_ON_ERROR(WriteRegion(0x24, region, pixels), kTag, "write new image RAM");
    return WriteRegion(0x26, region, pixels);
}

esp_err_t XzEpaperDisplay::FullRefresh(const Frame& pixels) {
    // Reset the partial waveform and voltage settings before the full refresh.
    ESP_RETURN_ON_ERROR(ResetPanel(), kTag, "reset for full mode");
#ifdef EPAPER_USE_SSD1680
    ESP_RETURN_ON_ERROR(LoadWaveform(kSsd1680FullWaveform), kTag, "SSD1680 full waveform");
#endif
    ESP_RETURN_ON_ERROR(WriteBothImages(kFullScreen, pixels), kTag, "write full baseline");
#ifdef EPAPER_USE_SSD1680
    return ActivateUpdate(0xC7);
#else
    return ActivateUpdate(0xF7);
#endif
}

esp_err_t XzEpaperDisplay::PartialRefresh() {
#ifdef EPAPER_USE_SSD1680
    // Keep both entire RAM planes coherent; the differential waveform drives only changed pixels.
    ESP_RETURN_ON_ERROR(WriteRegion(0x26, kFullScreen, displayed_), kTag,
                        "write SSD1680 old frame");
    ESP_RETURN_ON_ERROR(WriteRegion(0x24, kFullScreen, frame_), kTag, "write SSD1680 new frame");
    return ActivateUpdate(0xCF);
#else
    const Region region = FindChangedRegion();
    ESP_RETURN_ON_ERROR(WriteRegion(0x24, region, frame_), kTag, "write changed pixels");
#ifdef EPAPER_USE_SSD1683
    ESP_RETURN_ON_ERROR(ActivateUpdate(0xFF), kTag, "SSD1683 OTP partial refresh");
#else
    // Mode 2 uses the programmed waveform and shuts off the analog drive afterward.
    ESP_RETURN_ON_ERROR(ActivateUpdate(0xCF), kTag, "SSD1681 partial refresh");
#endif
    // Synchronize both planes so subsequent refreshes cannot re-drive stale transitions.
    return WriteBothImages(region, frame_);
#endif
}

void XzEpaperDisplay::Run() {
    bool panel_ready = false;
    bool panel_awake = false;
    unsigned partial_updates = 0;
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, panel_awake ? kPanelIdleDelay : portMAX_DELAY) == 0) {
            esp_err_t result = ESP_OK;
            if (partial_updates != 0) {
                ESP_LOGI(kTag, "Cleaning display after %u partial updates before sleep",
                         partial_updates);
                result = FullRefresh(displayed_);
            }
            if (result == ESP_OK) {
                result = Command(0x10, {0x01});
            }
            if (result == ESP_OK) {
                vTaskDelay(kPanelSleepDelay);
                ESP_LOGI(kTag, "Panel in deep sleep");
            } else {
                ESP_LOGW(kTag, "Panel standby failed: %s", esp_err_to_name(result));
            }
            panel_ready = result == ESP_OK;
            panel_awake = false;
            partial_updates = 0;
            continue;
        }

        vTaskDelay(kCoalesceDelay);
        ulTaskNotifyTake(pdTRUE, 0);
        {
            const std::lock_guard lock(frame_mutex_);
            if (panel_ready && displayed_ == pending_) {
                continue;
            }
            frame_ = pending_;
        }
        const bool full = !panel_ready || partial_updates >= kMaxPartialUpdates;
        esp_err_t result;
        if (full) {
            ESP_LOGI(kTag, "Full refresh (baseline or partial update limit)");
            result = FullRefresh(frame_);
        } else {
            result = partial_updates == 0 ? PreparePartial() : ESP_OK;
            if (result == ESP_OK) {
                result = PartialRefresh();
            }
        }
        panel_ready = result == ESP_OK;
        panel_awake = panel_ready;
        if (panel_ready) {
            displayed_ = frame_;
            partial_updates = full ? 0 : partial_updates + 1;
        } else {
            partial_updates = 0;
            ESP_LOGW(kTag, "Refresh failed: %s (BUSY=%d)", esp_err_to_name(result),
                     gpio_get_level(EPAPER_BUSY_GPIO));
        }
    }
}
