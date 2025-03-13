#include "wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "ml307_board.h"  // 引入ML307板级支持库，提供4G模块相关功能

#include "audio_codecs/no_audio_codec.h"  // 引入无音频编解码器库，用于无音频处理
#include "display/lcd_display.h"  // 引入LCD显示库，提供显示功能
#include "system_reset.h"  // 引入系统重置库，用于系统重置功能
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件
#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备
#include "led/single_led.h"  // 引入单LED库，用于控制LED

#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include <esp_lcd_panel_vendor.h>  // 引入LCD面板厂商库，提供LCD面板相关功能
#include <driver/i2c_master.h>  // 引入I2C主设备库，用于I2C通信
#include <wifi_station.h>  // 引入WiFi站库，用于WiFi连接管理

#define TAG "kevin-sp-v3"  // 定义日志标签，用于标识日志来源

LV_FONT_DECLARE(font_puhui_20_4);  // 声明普黑字体
LV_FONT_DECLARE(font_awesome_20_4);  // 声明Font Awesome字体

// KEVIN_SP_V3板子类，继承自WifiBoard
class KEVIN_SP_V3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;  // 显示I2C总线句柄
    Button boot_button_;  // 启动按钮对象
    LcdDisplay* display_;  // LCD显示对象

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_47;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚（未连接）
        buscfg.sclk_io_num = GPIO_NUM_21;  // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // QUADWP引脚（未连接）
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // QUADHD引脚（未连接）
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
            }
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();  // 开始监听
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();  // 停止监听
        });
    }

    // 初始化ST7789显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;  // CS引脚
        io_config.dc_gpio_num = GPIO_NUM_45;  // DC引脚
        io_config.spi_mode = 3;  // SPI模式
        io_config.pclk_hz = 80 * 1000 * 1000;  // 时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // LCD命令位数
        io_config.lcd_param_bits = 8;  // LCD参数位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 创建面板IO

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;  // 重置引脚（未连接）
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;  // RGB元素顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));  // 创建ST7789面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));  // 重置面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));  // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));  // 交换XY轴
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));  // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  // 反转颜色

        display_ = new SpiLcdDisplay(panel_io, panel,
                            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                            {
                                .text_font = &font_puhui_20_4,  // 设置文本字体
                                .icon_font = &font_awesome_20_4,  // 设置图标字体
                                .emoji_font = font_emoji_64_init(),  // 设置表情字体
                            });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Lamp"));  // 添加灯设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数
    KEVIN_SP_V3Board() : 
    boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "Initializing KEVIN_SP_V3 Board");  // 记录信息日志

        InitializeSpi();  // 初始化SPI总线
        InitializeButtons();  // 初始化按钮
        InitializeSt7789Display();  // 初始化ST7789显示屏
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }
    
    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 创建单LED对象
        return &led;
    }

    // 获取音频编解码器
    virtual AudioCodec *GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);  // 创建无音频编解码器对象
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display *GetDisplay() override {
        return display_;
    }
    
    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建PWM背光对象
        return &backlight;
    }
};

DECLARE_BOARD(KEVIN_SP_V3Board);  // 声明KEVIN_SP_V3板子