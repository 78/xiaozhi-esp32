#ifndef _T_DECK_PRO_A7682E_EPD_DISPLAY_H_
#define _T_DECK_PRO_A7682E_EPD_DISPLAY_H_

#include "config.h"
#include "lcd_display.h"

#include <driver/spi_master.h>

#include <cstddef>
#include <cstdint>
#include <mutex>

class A7682eEpdDisplay : public LcdDisplay {
public:
    A7682eEpdDisplay();
    ~A7682eEpdDisplay() override = default;

private:
    struct EpdWindow {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
    };

    static void LvglFlushCallback(lv_display_t* display, const lv_area_t* area,
                                  uint8_t* color_buffer);

    void InitializeGpio();
    bool InitializeSpi();
    bool InitializePanel();
    void HardwareReset();
    bool WaitWhileBusy(const char* operation);
    void SendCommand(uint8_t command);
    void SendData(uint8_t data);
    bool SendBytes(const uint8_t* data, size_t size);
    bool NormalizeWindow(const lv_area_t* area, EpdWindow* window) const;
    void SetPartialRamArea(const EpdWindow& window);
    void InitializePanelRegisters();
    bool WriteFullFrameBuffer(uint8_t command);
    bool WriteFrameRegion(uint8_t command, const EpdWindow& window);
    bool Refresh(const EpdWindow& window, bool full_refresh);
    bool PowerOn();
    bool PowerOff();
    void UpdateFrameBuffer(const lv_area_t* area, const uint8_t* color_buffer);

    static constexpr size_t kFrameBytes =
        ((T_DECK_PRO_A7682E_EPD_WIDTH + 7) / 8) * T_DECK_PRO_A7682E_EPD_HEIGHT;
    static constexpr size_t kLvglBufferBytes = T_DECK_PRO_A7682E_EPD_WIDTH *
                                               T_DECK_PRO_A7682E_EPD_HEIGHT *
                                               LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565);

    spi_device_handle_t spi_ = nullptr;
    uint8_t* frame_buffer_ = nullptr;
    uint8_t* lvgl_buffer_ = nullptr;
    std::mutex epd_mutex_;
    bool spi_bus_owned_ = false;
    bool panel_registers_initialized_ = false;
    bool power_is_on_ = false;
    bool panel_ready_ = false;
    bool first_content_refresh_ = true;
};

#endif  // _T_DECK_PRO_A7682E_EPD_DISPLAY_H_
