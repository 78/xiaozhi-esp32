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
    DISPLAY_TYPE_WLK_1_8_INCH,  // 沃乐康1.8英寸屏幕 (ST77916+QSPI+特殊初始化序列)
    DISPLAY_TYPE_JYC_1_5_INCH,  // 金逸晨1.5英寸屏幕 (ST77916+QSPI+特殊初始化序列)
    DISPLAY_TYPE_HXC_1_8_INCH,  // 华夏彩1.8英寸屏幕 (ST77916+QSPI+特殊初始化序列)
    DISPLAY_TYPE_HXC_1_15_INCH, // 华夏彩1.15英寸屏幕 (ST7789+SPI+使用标准库无需初始化序列)
} lcd_type_t;

// 通信方式枚举
typedef enum
{
    COMM_SPI,
    COMM_QSPI
} comm_type_t;

// 驱动类型枚举
typedef enum
{
    DRIVER_ST7789,
    DRIVER_ST77916
} driver_type_t;

// 屏幕配置项结构
typedef struct
{
    comm_type_t comm_type;
    driver_type_t driver_type;
    const void *init_cmds;   // 初始化命令序列
    uint16_t init_cmds_size; // 初始化命令数量
} lcd_config_item_t;

// LCD引脚配置结构体
typedef struct
{
    // SPI通信接口
    int spi_mosi_gpio; // 主输出从输入数据线(MOSI)
    int spi_miso_gpio; // 主输入从输出数据线(MISO) - 可选
    int spi_sclk_gpio; // 串行时钟线(SCLK)
    int spi_cs_gpio;   // 片选信号(CS)

    // QSPI通信接口
    int qspi_d0_gpio;   // QSPI数据线D0
    int qspi_d1_gpio;   // QSPI数据线D1
    int qspi_d2_gpio;   // QSPI数据线D2
    int qspi_d3_gpio;   // QSPI数据线D3
    int qspi_cs_gpio;   // QSPI片选信号
    int qspi_sclk_gpio; // QSPI时钟信号

    // ST7789面板驱动接口
    int st7789_dc_gpio;     // 数据/命令选择引脚(DC)
    int st7789_reset_gpio;  // 复位引脚(RESET)
    int st7789_pwr_en_gpio; // 电源使能引脚(PWR_EN) - 可选
    int st7789_bl_gpio;     // 背光控制引脚(BACKLIGHT)

    // ST77916面板驱动接口
    int st77916_dc_gpio;     // 数据/命令选择引脚(DC)
    int st77916_reset_gpio;  // 复位引脚(RESET)
    int st77916_pwr_en_gpio; // 电源使能引脚(PWR_EN) - 可选
    int st77916_bl_gpio;     // 背光控制引脚(BACKLIGHT)
    int st77916_te_gpio;     // 垂直同步信号引脚(TE) - 可选

    // 通用面板特性配置
    int width;     // 屏幕宽度
    int height;    // 屏幕高度
    int offset_x;  // X轴偏移量
    int offset_y;  // Y轴偏移量
    bool mirror_x; // X轴镜像
    bool mirror_y; // Y轴镜像
    bool swap_xy;  // XY轴交换
    int rotation;  // 屏幕旋转角度(0, 90, 180, 270)

} lcd_pin_config_t;

// 通信接口抽象基类
class ICommunicationInterface
{
public:
    virtual ~ICommunicationInterface() = default;
    virtual bool Initialize(esp_lcd_panel_io_handle_t *panel_io, const lcd_pin_config_t *pin_config) = 0;
};

// 驱动接口抽象基类
class IDisplayDriver
{
public:
    virtual ~IDisplayDriver() = default;
    virtual bool Initialize(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t *panel, const lcd_pin_config_t *pin_config, const lcd_config_item_t &config) = 0;
};

class FogSeekDisplayManager
{
private:
    esp_lcd_panel_io_handle_t panel_io_;
    esp_lcd_panel_handle_t panel_;
    std::unique_ptr<Backlight> backlight_;
    LcdDisplay *display_;

    // 获取屏幕配置
    static const lcd_config_item_t *GetLcdConfig(lcd_type_t lcd_type);

    // 创建通信接口
    std::unique_ptr<ICommunicationInterface> CreateCommunicationInterface(comm_type_t comm_type);

    // 创建驱动接口
    std::unique_ptr<IDisplayDriver> CreateDisplayDriver(driver_type_t driver_type, lcd_type_t lcd_type);

    // 通用初始化流程
    bool InitializeCommonComponents(const lcd_pin_config_t *pin_config);

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