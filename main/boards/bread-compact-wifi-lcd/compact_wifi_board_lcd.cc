#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h" // 包含ILI9341显示屏驱动
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h" // 包含GC9A01显示屏驱动
// GC9A01显示屏初始化命令
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0}, // 初始化命令
    {0xef, (uint8_t[]){0x00}, 0, 0}, // 初始化命令
    {0xb0, (uint8_t[]){0xc0}, 1, 0}, // 初始化命令
    {0xb1, (uint8_t[]){0x80}, 1, 0}, // 初始化命令
    {0xb2, (uint8_t[]){0x27}, 1, 0}, // 初始化命令
    {0xb3, (uint8_t[]){0x13}, 1, 0}, // 初始化命令
    {0xb6, (uint8_t[]){0x19}, 1, 0}, // 初始化命令
    {0xb7, (uint8_t[]){0x05}, 1, 0}, // 初始化命令
    {0xac, (uint8_t[]){0xc8}, 1, 0}, // 初始化命令
    {0xab, (uint8_t[]){0x0f}, 1, 0}, // 初始化命令
    {0x3a, (uint8_t[]){0x05}, 1, 0}, // 初始化命令
    {0xb4, (uint8_t[]){0x04}, 1, 0}, // 初始化命令
    {0xa8, (uint8_t[]){0x08}, 1, 0}, // 初始化命令
    {0xb8, (uint8_t[]){0x08}, 1, 0}, // 初始化命令
    {0xea, (uint8_t[]){0x02}, 1, 0}, // 初始化命令
    {0xe8, (uint8_t[]){0x2A}, 1, 0}, // 初始化命令
    {0xe9, (uint8_t[]){0x47}, 1, 0}, // 初始化命令
    {0xe7, (uint8_t[]){0x5f}, 1, 0}, // 初始化命令
    {0xc6, (uint8_t[]){0x21}, 1, 0}, // 初始化命令
    {0xc7, (uint8_t[]){0x15}, 1, 0}, // 初始化命令
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0}, // 初始化命令
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0}, // 初始化命令
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0}, // 初始化命令
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0}, // 初始化命令
};
#endif
 
#define TAG "CompactWifiBoardLCD" // 日志标签

LV_FONT_DECLARE(font_puhui_16_4); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_16_4); // 声明Font Awesome字体

// CompactWifiBoardLCD开发板类
class CompactWifiBoardLCD : public WifiBoard {
private:
    Button boot_button_; // 启动按钮
    LcdDisplay* display_; // LCD显示屏对象

    // 初始化SPI外设
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN; // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO引脚未使用
        buscfg.sclk_io_num = DISPLAY_CLK_PIN; // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO)); // 初始化SPI总线
    }

    // 初始化LCD显示屏
    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO"); // 日志：安装面板IO
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN; // CS引脚
        io_config.dc_gpio_num = DISPLAY_DC_PIN; // DC引脚
        io_config.spi_mode = DISPLAY_SPI_MODE; // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000; // 像素时钟频率
        io_config.trans_queue_depth = 10; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // 命令位宽
        io_config.lcd_param_bits = 8; // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io)); // 创建SPI面板IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver"); // 日志：安装LCD驱动
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN; // 复位引脚
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER; // RGB元素顺序
        panel_config.bits_per_pixel = 16; // 每像素位数
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel)); // 创建ILI9341面板
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel)); // 创建GC9A01面板
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds, // 初始化命令
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t), // 初始化命令大小
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel)); // 创建ST7789面板
#endif
        
        esp_lcd_panel_reset(panel); // 复位面板
        esp_lcd_panel_init(panel); // 初始化面板
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR); // 反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY); // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y); // 镜像显示
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config; // 供应商配置
#endif
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4, // 文本字体
                                        .icon_font = &font_awesome_16_4, // 图标字体
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(), // 表情字体
                                    });
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
            app.ToggleChatState(); // 切换聊天状态
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
#if DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC
        thing_manager.AddThing(iot::CreateThing("Backlight")); // 添加背光设备
#endif
    }

public:
    // 构造函数
    CompactWifiBoardLCD() :
        boot_button_(BOOT_BUTTON_GPIO) { // 初始化启动按钮
        InitializeSpi(); // 初始化SPI
        InitializeLcdDisplay(); // 初始化LCD显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备

#if DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC
        GetBacklight()->RestoreBrightness(); // 恢复背光亮度
#endif
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO); // 创建单LED对象
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        // 单工模式音频编解码器
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        // 双工模式音频编解码器
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }

#if DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC
    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT); // 创建PWM背光对象
        return &backlight;
    }
#endif
};

DECLARE_BOARD(CompactWifiBoardLCD); // 声明开发板