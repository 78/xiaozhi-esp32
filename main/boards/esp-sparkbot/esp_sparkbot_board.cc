#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "esp_sparkbot"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_20_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_20_4);  // 声明字体

// SparkBotEs8311AudioCodec类，继承自Es8311AudioCodec
class SparkBotEs8311AudioCodec : public Es8311AudioCodec {
private:    

public:
    // 构造函数
    SparkBotEs8311AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
                        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                        gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                             mclk,  bclk,  ws,  dout,  din,pa_pin,  es8311_addr,  use_mclk = true) {}

    // 启用或禁用音频输出
    void EnableOutput(bool enable) override {
        if (enable == output_enabled_) {
            return;  // 如果状态未改变，则直接返回
        }
        if (enable) {
            Es8311AudioCodec::EnableOutput(enable);  // 启用音频输出
        } else {
           // 由于显示IO和PA IO冲突，禁用时不执行任何操作
        }
    }
};

// EspSparkBot类，继承自WifiBoard
class EspSparkBot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    Button boot_button_;  // 启动按钮
    Display* display_;  // LCD显示屏对象

    // 初始化I2C总线
    void InitializeI2c() {
        // 配置I2C总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,  // I2C端口
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
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚未使用
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;  // 时钟引脚
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

    // 初始化显示屏
    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 初始化液晶屏控制IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;  // 片选引脚
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;  // 数据/命令选择引脚
        io_config.spi_mode = 0;  // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // 命令位宽
        io_config.lcd_param_bits = 8;  // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 创建SPI控制IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;  // 复位引脚未使用
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;  // RGB顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));  // 创建ST7789驱动

        esp_lcd_panel_reset(panel);  // 复位液晶屏
        esp_lcd_panel_init(panel);  // 初始化液晶屏
        esp_lcd_panel_invert_color(panel, true);  // 颜色反转
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
        thing_manager.AddThing(iot::CreateThing("Chassis"));  // 添加底盘设备
    }

public:
    // 构造函数
    EspSparkBot() : boot_button_(BOOT_BUTTON_GPIO) {  // 初始化启动按钮
        InitializeI2c();  // 初始化I2C总线
        InitializeSpi();  // 初始化SPI总线
        InitializeDisplay();  // 初始化显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
         static SparkBotEs8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 返回SparkBotEs8311音频编解码器对象
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

DECLARE_BOARD(EspSparkBot);  // 声明EspSparkBot板子