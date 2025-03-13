#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "display/no_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define TAG "MovecallMojiESP32S3"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_20_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_20_4);

// CustomLcdDisplay 类继承自 SpiLcdDisplay，用于管理自定义 LCD 显示屏
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    // 构造函数，初始化显示屏并设置样式
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4,  // 设置文本字体
                        .icon_font = &font_awesome_20_4,  // 设置图标字体
                        .emoji_font = font_emoji_64_init(),  // 设置表情字体
                    }) {

        DisplayLockGuard lock(this);  // 加锁，确保线程安全
        // 由于屏幕是圆的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.33, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.33, 0);
    }
};

// MovecallMojiESP32S3 类继承自 WifiBoard，用于管理整个硬件平台
class MovecallMojiESP32S3 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C 总线句柄
    Button boot_button_;  // 启动按钮
    Display* display_;  // 显示屏对象

    // 初始化音频编解码器的 I2C 总线
    void InitializeCodecI2c() {
        // 配置 I2C 总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA 引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉电阻
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化 I2C 总线
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");  // 日志输出
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));  // 配置 SPI 总线
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 GC9A01 显示屏
    void InitializeGc9a01Display() {
        ESP_LOGI(TAG, "Init GC9A01 display");  // 日志输出

        ESP_LOGI(TAG, "Install panel IO");  // 日志输出
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);  // 配置 SPI 面板 IO
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;  // 设置 SPI 时钟频率
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));  // 初始化 SPI 面板 IO
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver");  // 日志输出
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;  // 复位引脚
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;  // RGB 字节序
        panel_config.bits_per_pixel = 16;  // 每像素位数

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));  // 初始化 GC9A01 面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));  // 重置面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));  // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));  // 反转颜色
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));  // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));  // 打开显示

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,  // 设置文本字体
                                        .icon_font = &font_awesome_20_4,  // 设置图标字体
                                        .emoji_font = font_emoji_64_init(),  // 设置表情字体
                                    });  // 创建 SpiLcdDisplay 对象
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 按钮点击事件
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置 WiFi 配置
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
    // 构造函数，初始化硬件平台
    MovecallMojiESP32S3() : boot_button_(BOOT_BUTTON_GPIO) {  
        InitializeCodecI2c();  // 初始化音频编解码器的 I2C 总线
        InitializeSpi();  // 初始化 SPI 总线
        InitializeGc9a01Display();  // 初始化 GC9A01 显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取 LED 对象
    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO);  // 创建单 LED 对象
        return &led_strip;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建 PWM 背光对象
        return &backlight;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建 ES8311 音频编解码器对象
        return &audio_codec;
    }
};

// 声明 MovecallMojiESP32S3 板子
DECLARE_BOARD(MovecallMojiESP32S3);