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
#include <driver/spi_common.h>

#if defined(LCD_ILI9341_240X320) || defined(LCD_ILI9341_240X320_NO_IPS)
#include "esp_lcd_ili9341.h"
#endif

#define TAG "DuChatX"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_16_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_16_4);  // 声明字体

// DuChatX类，继承自WifiBoard
class DuChatX : public WifiBoard {
private:
    Button boot_button_;  // 启动按钮
    LcdDisplay* display_;  // LCD显示屏对象

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚未使用
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;  // 时钟引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // 未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // 未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化LCD显示屏
    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 初始化液晶屏控制IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;  // 片选引脚
        io_config.dc_gpio_num = DISPLAY_DC_PIN;  // 数据/命令选择引脚
        io_config.spi_mode = 0;  // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // 命令位宽
        io_config.lcd_param_bits = 8;  // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 创建SPI控制IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // 复位引脚
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;  // RGB顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
#if defined(LCD_ILI9341_240X320) || defined(LCD_ILI9341_240X320_NO_IPS)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));  // 创建ILI9341驱动
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));  // 创建ST7789驱动
#endif

        esp_lcd_panel_reset(panel);  // 复位液晶屏
        esp_lcd_panel_init(panel);  // 初始化液晶屏
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);  // 颜色反转
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,  // 文本字体
                                        .icon_font = &font_awesome_16_4,  // 图标字体
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),  // 表情字体
                                    });
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

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数
    DuChatX() :
        boot_button_(BOOT_BUTTON_GPIO) {  // 初始化启动按钮
        InitializeSpi();  // 初始化SPI总线
        InitializeLcdDisplay();  // 初始化LCD显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 单色LED
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);  // 无音频编解码器
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

DECLARE_BOARD(DuChatX);  // 声明DuChatX板子