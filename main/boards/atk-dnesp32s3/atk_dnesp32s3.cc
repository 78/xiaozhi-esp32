#include "wifi_board.h"
#include "es8388_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "atk_dnesp32s3" // 日志标签

LV_FONT_DECLARE(font_puhui_20_4); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_20_4); // 声明Font Awesome字体

// XL9555 I2C扩展芯片类
class XL9555 : public I2cDevice {
public:
    XL9555(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x06, 0x03); // 配置端口0为输出模式
        WriteReg(0x07, 0xF0); // 配置端口1为输出模式
    }

    // 设置输出状态
    void SetOutputState(uint8_t bit, uint8_t level) {
        uint16_t data;
        if (bit < 8) {
            data = ReadReg(0x02); // 读取端口0的状态
        } else {
            data = ReadReg(0x03); // 读取端口1的状态
            bit -= 8;
        }

        data = (data & ~(1 << bit)) | (level << bit); // 设置指定bit的状态

        if (bit < 8) {
            WriteReg(0x02, data); // 写回端口0的状态
        } else {
            WriteReg(0x03, data); // 写回端口1的状态
        }
    }
};

// ATK DNESP32S3开发板类
class atk_dnesp32s3 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C总线句柄
    Button boot_button_; // 启动按钮
    LcdDisplay* display_; // LCD显示屏
    XL9555* xl9555_; // XL9555扩展芯片

    // 初始化I2C外设
    void InitializeI2c() {
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0, // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_)); // 创建I2C总线

        // 初始化XL9555
        xl9555_ = new XL9555(i2c_bus_, 0x20); // XL9555地址为0x20
    }

    // 初始化SPI外设
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = LCD_MOSI_PIN; // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO引脚未使用
        buscfg.sclk_io_num = LCD_SCLK_PIN; // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO)); // 初始化SPI总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening(); // 开始监听
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening(); // 停止监听
        });
    }

    // 初始化ST7789显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        // 液晶屏控制IO初始化
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS_PIN; // CS引脚
        io_config.dc_gpio_num = LCD_DC_PIN; // DC引脚
        io_config.spi_mode = 0; // SPI模式
        io_config.pclk_hz = 20 * 1000 * 1000; // 像素时钟频率
        io_config.trans_queue_depth = 7; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // 命令位宽
        io_config.lcd_param_bits = 8; // 参数位宽
        esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io); // 创建SPI面板IO

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC; // 复位引脚未使用
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // RGB元素顺序
        panel_config.bits_per_pixel = 16; // 每像素位数
        panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG, // 数据字节序
        esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel); // 创建ST7789面板
        
        esp_lcd_panel_reset(panel); // 复位面板
        xl9555_->SetOutputState(8, 1); // 设置XL9555输出状态
        xl9555_->SetOutputState(2, 0); // 设置XL9555输出状态

        esp_lcd_panel_init(panel); // 初始化面板
        esp_lcd_panel_invert_color(panel, true); // 反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY); // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y); // 镜像显示
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4, // 文本字体
                                        .icon_font = &font_awesome_20_4, // 图标字体
                                        .emoji_font = font_emoji_64_init(), // 表情字体
                                    });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
    }

public:
    atk_dnesp32s3() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化I2C
        InitializeSpi(); // 初始化SPI
        InitializeSt7789Display(); // 初始化ST7789显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO); // 内置LED
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8388AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8388_ADDR
        );
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(atk_dnesp32s3); // 声明开发板