#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include "display/lcd_display.h"
#include "boards/common/backlight.h"
#include "device_state.h"
#include <memory>

// 屏幕尺寸/型号枚举类型
typedef enum
{
    LCD_TYPE_WLK_1_8_INCH, // 沃乐康1.8英寸屏幕
    LCD_TYPE_JYC_1_5_INCH, // 金逸晨1.5英寸屏幕
    LCD_TYPE_HXC_1_8_INCH, // 华夏彩1.8英寸屏幕
} lcd_type_t;

// LCD引脚配置结构体
typedef struct
{
    int io0_gpio;
    int io1_gpio;
    int scl_gpio;
    int io2_gpio;
    int io3_gpio;
    int cs_gpio;
    int dc_gpio;
    int reset_gpio;
    int im0_gpio;
    int im2_gpio;
    int bl_gpio;

    // 屏幕显示配置参数
    int width;
    int height;
    int offset_x;
    int offset_y;
    bool mirror_x;
    bool mirror_y;
    bool swap_xy;
} lcd_pin_config_t;

class FogSeekDisplayManager
{
private:
    esp_lcd_panel_io_handle_t panel_io_;
    esp_lcd_panel_handle_t panel_;
    std::unique_ptr<Backlight> backlight_;
    LcdDisplay *display_;

public:
    FogSeekDisplayManager();
    ~FogSeekDisplayManager();

    // 初始化显示管理器，添加屏幕类型参数和引脚配置
    void Initialize(lcd_type_t lcd_type, const lcd_pin_config_t *pin_config);

    // 获取显示对象
    LcdDisplay *GetDisplay() { return display_; }

    // 设置亮度
    void SetBrightness(int percent);

    // 恢复亮度
    void RestoreBrightness();

    // 设置状态文本
    void SetStatus(const char *status);

    // 设置聊天消息
    void SetChatMessage(const char *sender, const char *message);

    // 处理设备状态变更
    void HandleDeviceState(DeviceState current_state);
};