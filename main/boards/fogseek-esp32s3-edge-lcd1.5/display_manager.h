#ifndef _DISPLAY_MANAGER_H_
#define _DISPLAY_MANAGER_H_

#include "display.h"
#include "backlight.h"
#include "device_state.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class DisplayManager
{
public:
    DisplayManager();
    ~DisplayManager();

    void Initialize();
    void SetBrightness(uint8_t brightness);
    void RestoreBrightness();
    void SetStatus(const char *status);
    void SetChatMessage(const char *sender, const char *message);

    // 设备状态处理
    void HandleDeviceState(DeviceState current_state);

    Display *GetDisplay() { return display_; }

private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display *display_ = nullptr;
    Backlight *backlight_ = nullptr;
};

#endif