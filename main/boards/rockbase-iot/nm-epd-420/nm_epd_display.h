#ifndef NM_EPD_DISPLAY_H_
#define NM_EPD_DISPLAY_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <lvgl.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "lcd_display.h"

// GYE042A87 4.2" 400x300 black/white e-paper panel (SSD1683 controller) as
// fitted on the RockBase NM-EPD-420-BW. LVGL renders into a PSRAM-backed
// RGB565 surface; a flush callback converts dirty areas to a 1bpp shadow
// frame and a low-priority task pushes full or partial refreshes to the
// panel, so slow EPD waveforms never block the UI task.
//
// Panel command flow follows the RockBase nm-epd420-bw-demo (zectrix_epd):
// 0x24/0x26 address the controller's current/previous image RAM banks,
// 0x22+0x20 triggers a fast full (0xD7) or partial (0xFC) update, and after
// each partial update the window is mirrored into both RAM banks so the next
// differential waveform keeps a valid baseline.

struct NmEpdPins {
    gpio_num_t cs;
    gpio_num_t dc;
    gpio_num_t rst;
    gpio_num_t busy;
    gpio_num_t mosi;
    gpio_num_t sck;
    spi_host_device_t spi_host;
};

struct NmEpdRect {
    int x;
    int y;
    int w;
    int h;
};

class NmEpdDisplay : public LcdDisplay {
public:
    NmEpdDisplay(int width, int height, const NmEpdPins& pins);
    ~NmEpdDisplay() override;

    // Force the next panel update to be a full refresh (clears ghosting).
    void RequestFullRefresh();
    bool IsRefreshPending();

    // Reserve the lower ~50% of the screen for wrapped multi-line chat text
    // instead of the default single-line scrolling subtitle.
    void SetupUI() override;
    void SetChatMessage(const char* role, const char* content) override;
    // E-paper: do not mirror device state ("listening"/"speaking"/...) into
    // the top status bar on every transition; show only the wall clock.
    void SetStatus(const char* status) override;

private:
    const NmEpdPins pins_;
    const int width_;
    const int height_;
    const int frame_bytes_;  // width * height / 8

    spi_device_handle_t spi_ = nullptr;
    uint8_t* frame_ = nullptr;       // current 1bpp frame composed from LVGL
    uint8_t* prev_frame_ = nullptr;  // last frame actually sent to the panel
    uint8_t* tx_frame_ = nullptr;    // snapshot being transmitted
    uint8_t* dma_buffer_ = nullptr;  // internal-RAM bounce buffer for SPI DMA

    SemaphoreHandle_t dirty_mutex_ = nullptr;
    TaskHandle_t refresh_task_ = nullptr;
    NmEpdRect dirty_ = {0, 0, 0, 0};
    bool pending_ = false;
    bool force_full_refresh_ = false;
    bool prev_valid_ = false;
    int partial_since_full_ = 0;
    TickType_t last_refresh_tick_ = 0;
    // Deferred full refresh: while text streams in we keep doing partial
    // updates only, and run the pending full refresh once the UI has been
    // idle for a moment (avoids a ~2.5 s full flash mid-conversation).
    bool idle_full_due_ = false;
    TickType_t last_activity_tick_ = 0;
    int last_clock_min_ = -1;  // last HH:MM minute shown in the status bar

    static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p);

    void SpiInit();
    void Transmit(const uint8_t* data, size_t len);
    void SendCommand(uint8_t command);
    void SendData(uint8_t data);
    void SendBytes(const uint8_t* data, size_t len);
    void WaitBusy(const char* operation);
    void HardwareReset();
    void SetRamArea(int x, int y, int w, int h);
    void InitPanel();
    void RefreshFull(const uint8_t* frame);
    void RefreshPartial(const NmEpdRect& rect, const uint8_t* frame);

    void StartRefreshTask();
    static void RefreshTaskEntry(void* arg);
    void RefreshTaskLoop();
};

#endif  // NM_EPD_DISPLAY_H_
