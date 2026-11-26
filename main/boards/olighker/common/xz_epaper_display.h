#pragma once

#include "config.h"
#include "display/lcd_display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <array>
#include <initializer_list>
#include <mutex>

class XzEpaperDisplay final : public LcdDisplay {
public:
    XzEpaperDisplay();
    ~XzEpaperDisplay() override;
    bool Initialize();
    void SetupUI() override;
    void SetChatMessage(const char* role, const char* content) override;
    void RemoveChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetHideSubtitle(bool hide) override;

private:
    static_assert(DISPLAY_WIDTH % 8 == 0 && EPAPER_RAM_WIDTH % 8 == 0);
#ifdef EPAPER_ROTATE_270
    static_assert(DISPLAY_WIDTH == EPAPER_RAM_HEIGHT && DISPLAY_HEIGHT == EPAPER_RAM_WIDTH);
#else
    static_assert(DISPLAY_WIDTH == EPAPER_RAM_WIDTH && DISPLAY_HEIGHT == EPAPER_RAM_HEIGHT);
#endif
    static constexpr size_t kRowBytes = EPAPER_RAM_WIDTH / 8;
    static constexpr size_t kStride = (DISPLAY_WIDTH / 8 + LV_DRAW_BUF_STRIDE_ALIGN - 1) /
                                      LV_DRAW_BUF_STRIDE_ALIGN * LV_DRAW_BUF_STRIDE_ALIGN;
    static constexpr size_t kPaletteBytes = 8;
    using Frame = std::array<uint8_t, kRowBytes * EPAPER_RAM_HEIGHT>;
#ifdef EPAPER_USE_SSD1683
    static constexpr size_t kSpiBufferSize = 4000;
#else
    static constexpr size_t kSpiBufferSize = kRowBytes * EPAPER_RAM_HEIGHT;
#endif
    static_assert(kSpiBufferSize >= kRowBytes);
    struct Region {
        int left_byte;
        int right_byte;
        int top;
        int bottom;
    };
    static constexpr Region kFullScreen{0, kRowBytes - 1, 0, EPAPER_RAM_HEIGHT - 1};

    alignas(LV_DRAW_BUF_ALIGN)
        std::array<uint8_t, kStride * DISPLAY_HEIGHT + kPaletteBytes> draw_buffer_{};
    Frame pending_{};
    Frame frame_{};
    Frame displayed_{};
    alignas(4) std::array<uint8_t, kSpiBufferSize> spi_pixels_{};
    std::mutex frame_mutex_;
    bool pending_valid_ = false;
    esp_lcd_panel_io_handle_t io_ = nullptr;
    TaskHandle_t refresh_task_ = nullptr;
    bool bus_initialized_ = false;
    lv_timer_t* subtitle_timer_ = nullptr;
    std::string subtitle_;
    size_t subtitle_offset_ = 0;
    bool assistant_subtitle_ = false;
    bool clear_after_pages_ = false;

    void ShowSubtitlePage();
    void ResetSubtitle();
    static void Flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    esp_err_t Command(uint8_t command, std::initializer_list<uint8_t> data = {});
    esp_err_t WaitIdle();
    esp_err_t ResetPanel();
    esp_err_t LoadWaveform(const uint8_t* waveform);
    esp_err_t PreparePartial();
    esp_err_t ActivateUpdate(uint8_t mode);
    Region FindChangedRegion() const;
    esp_err_t SetWindow(const Region& region);
    esp_err_t WriteRegion(uint8_t command, const Region& region, const Frame& pixels);
    esp_err_t WriteBothImages(const Region& region, const Frame& pixels);
    esp_err_t FullRefresh(const Frame& pixels);
    esp_err_t PartialRefresh();
    void Run();
};
