#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "pin_config.h"

#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "esp_lcd_gc9503.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io_additions.h>

#define TAG "Yuying_313lcd" // 日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

// Yuying_313lcd 类，继承自 WifiBoard
class Yuying_313lcd : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_; // I2C总线句柄，用于音频编解码器
    Button boot_button_;                    // 按钮对象，用于处理按钮事件
    LcdDisplay* display_;                   // LCD显示对象

    // 初始化 GC9503V RGB 显示屏
    void InitializeRGB_GC9503V_Display() {
        ESP_LOGI(TAG, "Init GC9503V"); // 打印日志

        esp_lcd_panel_io_handle_t panel_io = nullptr; // LCD面板IO句柄

        // 安装3线SPI面板IO
        ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_GPIO,          // CS引脚类型为GPIO
            .cs_gpio_num = GC9503V_LCD_IO_SPI_CS_1, // CS引脚号
            .scl_io_type = IO_TYPE_GPIO,         // SCL引脚类型为GPIO
            .scl_gpio_num = GC9503V_LCD_IO_SPI_SCL_1, // SCL引脚号
            .sda_io_type = IO_TYPE_GPIO,         // SDA引脚类型为GPIO
            .sda_gpio_num = GC9503V_LCD_IO_SPI_SDO_1, // SDA引脚号
            .io_expander = NULL,                // 未使用IO扩展器
        };
        // 配置3线SPI面板IO
        esp_lcd_panel_io_3wire_spi_config_t io_config = GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        (esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io)); // 创建3线SPI面板IO

        // 安装RGB LCD面板驱动
        ESP_LOGI(TAG, "Install RGB LCD panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL; // LCD面板句柄
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_PLL160M, // 时钟源为PLL160M
            .timings = GC9503_376_960_PANEL_60HZ_RGB_TIMING(), // 时序配置
            .data_width = 16, // RGB565并行模式，数据宽度为16位
            .bits_per_pixel = 16, // 每个像素的位数为16
            .num_fbs = GC9503V_LCD_RGB_BUFFER_NUMS, // 帧缓冲区数量
            .bounce_buffer_size_px = GC9503V_LCD_H_RES * GC9503V_LCD_RGB_BOUNCE_BUFFER_HEIGHT, // 弹跳缓冲区大小
            .dma_burst_size = 64, // DMA突发大小
            .hsync_gpio_num = GC9503V_PIN_NUM_HSYNC, // HSYNC引脚号
            .vsync_gpio_num = GC9503V_PIN_NUM_VSYNC, // VSYNC引脚号
            .de_gpio_num = GC9503V_PIN_NUM_DE, // DE引脚号
            .pclk_gpio_num = GC9503V_PIN_NUM_PCLK, // PCLK引脚号
            .disp_gpio_num = GC9503V_PIN_NUM_DISP_EN, // 显示使能引脚号
            .data_gpio_nums = { // 数据引脚号
                GC9503V_PIN_NUM_DATA0,
                GC9503V_PIN_NUM_DATA1,
                GC9503V_PIN_NUM_DATA2,
                GC9503V_PIN_NUM_DATA3,
                GC9503V_PIN_NUM_DATA4,
                GC9503V_PIN_NUM_DATA5,
                GC9503V_PIN_NUM_DATA6,
                GC9503V_PIN_NUM_DATA7,
                GC9503V_PIN_NUM_DATA8,
                GC9503V_PIN_NUM_DATA9,
                GC9503V_PIN_NUM_DATA10,
                GC9503V_PIN_NUM_DATA11,
                GC9503V_PIN_NUM_DATA12,
                GC9503V_PIN_NUM_DATA13,
                GC9503V_PIN_NUM_DATA14,
                GC9503V_PIN_NUM_DATA15,
            },
            .flags= {
                .fb_in_psram = true, // 帧缓冲区分配在PSRAM中
            }
        };

        ESP_LOGI(TAG, "Initialize RGB LCD panel");

        // 配置GC9503供应商特定配置
        gc9503_vendor_config_t vendor_config = {
            .rgb_config = &rgb_config, // RGB配置
            .flags = {
                .mirror_by_cmd = 0, // 不通过命令控制镜像
                .auto_del_panel_io = 1, // 自动删除面板IO
            },
        };
        // 配置LCD面板设备
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = -1, // 复位引脚号（未使用）
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // RGB元素顺序
            .bits_per_pixel = 16, // 每个像素的位数
            .vendor_config = &vendor_config, // 供应商配置
        };
        // 创建GC9503面板
        (esp_lcd_new_panel_gc9503(panel_io, &panel_config, &panel_handle));
        (esp_lcd_panel_reset(panel_handle)); // 复位面板
        (esp_lcd_panel_init(panel_handle)); // 初始化面板

        // 创建RGB LCD显示对象
        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_30_4, // 文本字体
                                      .icon_font = &font_awesome_30_4, // 图标字体
                                      .emoji_font = font_emoji_64_init(), // 表情字体
                                  });
    }

    // 初始化音频编解码器I2C总线
    void InitializeCodecI2c() {
        // 配置I2C总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0, // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA引脚号
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL引脚号
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };
        // 创建I2C主总线
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // 初始化按钮
    void InitializeButtons() {
        // 设置按钮点击事件
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
        });
        // 设置按钮按下事件
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening(); // 开始监听
        });
        // 设置按钮释放事件
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening(); // 停止监听
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight")); // 添加背光设备
    }

public:
    // 构造函数
    Yuying_313lcd() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c(); // 初始化音频编解码器I2C总线
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
        InitializeRGB_GC9503V_Display(); // 初始化RGB显示屏
        GetBacklight()->RestoreBrightness(); // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

// 声明板级对象
DECLARE_BOARD(Yuying_313lcd);