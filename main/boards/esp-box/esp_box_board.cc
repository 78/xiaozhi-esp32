#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "EspBoxBoard"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_20_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_20_4);  // 声明字体

// ILI9341显示屏的初始化命令
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},  // 设置Gamma校正
    {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},  // 设置电源控制
    {0xC5, (uint8_t []){0xD0}, 1, 0},  // 设置VCOM控制
    {0xC1, (uint8_t []){0x02}, 1, 0},  // 设置电源控制2
    {0xB4, (uint8_t []){0x02}, 1, 0},  // 设置显示反转
    {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},  // 设置正极性Gamma校正
    {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},  // 设置负极性Gamma校正

    {0xB1, (uint8_t []){00, 0x1B}, 2, 0},  // 设置帧率控制
    {0x36, (uint8_t []){0x08}, 1, 0},  // 设置内存访问控制
    {0x3A, (uint8_t []){0x55}, 1, 0},  // 设置像素格式
    {0xB7, (uint8_t []){0x06}, 1, 0},  // 设置入口模式

    {0x11, (uint8_t []){0}, 0x80, 0},  // 退出睡眠模式
    {0x29, (uint8_t []){0}, 0x80, 0},  // 打开显示

    {0, (uint8_t []){0}, 0xff, 0},  // 结束命令
};

// EspBox3Board类，继承自WifiBoard
class EspBox3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    Button boot_button_;  // 启动按钮
    LcdDisplay* display_;  // LCD显示屏对象

    // 初始化I2C总线
    void InitializeI2c() {
        // 配置I2C总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,  // I2C端口
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 初始化I2C总线
    }

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_6;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚未使用
        buscfg.sclk_io_num = GPIO_NUM_7;  // 时钟引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // 未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // 未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 如果设备正在启动且未连接WiFi，则重置WiFi配置
            }
            app.ToggleChatState();  // 切换聊天状态
        });
    }

    // 初始化ILI9341显示屏
    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 初始化液晶屏控制IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_5;  // 片选引脚
        io_config.dc_gpio_num = GPIO_NUM_4;  // 数据/命令选择引脚
        io_config.spi_mode = 0;  // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // 命令位宽
        io_config.lcd_param_bits = 8;  // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 创建SPI控制IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const ili9341_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],  // 初始化命令
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),  // 初始化命令数量
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_48;  // 复位引脚
        panel_config.flags.reset_active_high = 0,  // 复位信号低电平有效
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;  // RGB顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        panel_config.vendor_config = (void *)&vendor_config;  // 供应商配置
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));  // 创建ILI9341驱动

        esp_lcd_panel_reset(panel);  // 复位液晶屏
        esp_lcd_panel_init(panel);  // 初始化液晶屏
        esp_lcd_panel_invert_color(panel, false);  // 颜色不反转
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,  // 文本字体
                                        .icon_font = &font_awesome_20_4,  // 图标字体
                                        .emoji_font = font_emoji_64_init(),  // 表情字体
                                    });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数
    EspBox3Board() : boot_button_(BOOT_BUTTON_GPIO) {  // 初始化启动按钮
        InitializeI2c();  // 初始化I2C总线
        InitializeSpi();  // 初始化SPI总线
        InitializeIli9341Display();  // 初始化ILI9341显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);  // 返回Box音频编解码器对象
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // PWM背光
        return &backlight;
    }
};

DECLARE_BOARD(EspBox3Board);  // 声明EspBox3Board板子