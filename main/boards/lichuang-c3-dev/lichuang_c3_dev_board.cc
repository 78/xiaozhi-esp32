#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "LichuangC3DevBoard" // 日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

// LichuangC3DevBoard 类，继承自 WifiBoard
class LichuangC3DevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_; // I2C总线句柄，用于音频编解码器
    Button boot_button_;                    // 按钮对象，用于处理按钮事件
    LcdDisplay* display_;                   // LCD显示对象

    // 初始化I2C总线
    void InitializeI2c() {
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

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN; // MOSI引脚号
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO引脚未使用
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN; // SCK引脚号
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        // 初始化SPI总线
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 初始化按钮
    void InitializeButtons() {
        // 设置按钮点击事件
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
            app.ToggleChatState(); // 切换聊天状态
        });
    }

    // 初始化ST7789显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr; // LCD面板IO句柄
        esp_lcd_panel_handle_t panel = nullptr; // LCD面板句柄

        // 初始化液晶屏控制IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN; // CS引脚号
        io_config.dc_gpio_num = DISPLAY_DC_PIN; // DC引脚号
        io_config.spi_mode = 2; // SPI模式
        io_config.pclk_hz = 80 * 1000 * 1000; // 时钟频率
        io_config.trans_queue_depth = 10; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // LCD命令位数
        io_config.lcd_param_bits = 8; // LCD参数位数
        // 创建SPI面板IO
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC; // 复位引脚未使用
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // RGB元素顺序
        panel_config.bits_per_pixel = 16; // 每个像素的位数
        // 创建ST7789面板
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        // 复位面板
        esp_lcd_panel_reset(panel);
        // 初始化面板
        esp_lcd_panel_init(panel);
        // 反转颜色
        esp_lcd_panel_invert_color(panel, true);
        // 交换XY轴
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        // 镜像显示
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        // 创建SPI LCD显示对象
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4, // 文本字体
                                        .icon_font = &font_awesome_16_4, // 图标字体
                                        .emoji_font = font_emoji_32_init(), // 表情字体
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
    LichuangC3DevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化I2C总线
        InitializeSpi(); // 初始化SPI总线
        InitializeSt7789Display(); // 初始化ST7789显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
        GetBacklight()->SetBrightness(100); // 设置背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, // I2C总线句柄
            I2C_NUM_0, // I2C端口号
            AUDIO_INPUT_SAMPLE_RATE, // 音频输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE, // 音频输出采样率
            AUDIO_I2S_GPIO_MCLK, // I2S主时钟引脚
            AUDIO_I2S_GPIO_BCLK, // I2S位时钟引脚
            AUDIO_I2S_GPIO_WS, // I2S字选择引脚
            AUDIO_I2S_GPIO_DOUT, // I2S数据输出引脚
            AUDIO_I2S_GPIO_DIN, // I2S数据输入引脚
            AUDIO_CODEC_PA_PIN, // 音频编解码器功放引脚
            AUDIO_CODEC_ES8311_ADDR); // 音频编解码器地址
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
DECLARE_BOARD(LichuangC3DevBoard);