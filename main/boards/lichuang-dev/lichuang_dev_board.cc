#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
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

#define TAG "LichuangDevBoard" // 日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

// Pca9557 类，继承自 I2cDevice，用于控制 PCA9557 I2C 扩展芯片
class Pca9557 : public I2cDevice {
public:
    // 构造函数，初始化 PCA9557
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x01, 0x03); // 配置 PCA9557 的输出寄存器
        WriteReg(0x03, 0xf8); // 配置 PCA9557 的配置寄存器
    }

    // 设置输出状态
    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01); // 读取当前输出状态
        data = (data & ~(1 << bit)) | (level << bit); // 设置指定位的状态
        WriteReg(0x01, data); // 写回输出状态
    }
};

// LichuangDevBoard 类，继承自 WifiBoard
class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C 总线句柄
    i2c_master_dev_handle_t pca9557_handle_; // PCA9557 设备句柄
    Button boot_button_; // 按钮对象，用于处理按钮事件
    LcdDisplay* display_; // LCD 显示对象
    Pca9557* pca9557_; // PCA9557 对象

    // 初始化 I2C 总线
    void InitializeI2c() {
        // 配置 I2C 总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1, // I2C 端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA 引脚号
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL 引脚号
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };
        // 创建 I2C 主总线
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // 初始化 PCA9557
        pca9557_ = new Pca9557(i2c_bus_, 0x19); // 创建 PCA9557 对象
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_40; // MOSI 引脚号
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO 引脚未使用
        buscfg.sclk_io_num = GPIO_NUM_41; // SCK 引脚号
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP 引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD 引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        // 初始化 SPI 总线
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 初始化按钮
    void InitializeButtons() {
        // 设置按钮点击事件
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置 WiFi 配置
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

    // 初始化 ST7789 显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr; // LCD 面板 IO 句柄
        esp_lcd_panel_handle_t panel = nullptr; // LCD 面板句柄

        // 初始化液晶屏控制 IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC; // CS 引脚未使用
        io_config.dc_gpio_num = GPIO_NUM_39; // DC 引脚号
        io_config.spi_mode = 2; // SPI 模式
        io_config.pclk_hz = 80 * 1000 * 1000; // 时钟频率
        io_config.trans_queue_depth = 10; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // LCD 命令位数
        io_config.lcd_param_bits = 8; // LCD 参数位数
        // 创建 SPI 面板 IO
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片 ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC; // 复位引脚未使用
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // RGB 元素顺序
        panel_config.bits_per_pixel = 16; // 每个像素的位数
        // 创建 ST7789 面板
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        // 复位面板
        esp_lcd_panel_reset(panel);
        // 设置 PCA9557 输出状态
        pca9557_->SetOutputState(0, 0);

        // 初始化面板
        esp_lcd_panel_init(panel);
        // 反转颜色
        esp_lcd_panel_invert_color(panel, true);
        // 交换 XY 轴
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        // 镜像显示
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        // 创建 SPI LCD 显示对象
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
        thing_manager.AddThing(iot::CreateThing("Backlight")); // 添加背光设备
    }

public:
    // 构造函数
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化 I2C 总线
        InitializeSpi(); // 初始化 SPI 总线
        InitializeSt7789Display(); // 初始化 ST7789 显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
        GetBacklight()->RestoreBrightness(); // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, // I2C 总线句柄
            AUDIO_INPUT_SAMPLE_RATE, // 音频输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE, // 音频输出采样率
            AUDIO_I2S_GPIO_MCLK, // I2S 主时钟引脚
            AUDIO_I2S_GPIO_BCLK, // I2S 位时钟引脚
            AUDIO_I2S_GPIO_WS, // I2S 字选择引脚
            AUDIO_I2S_GPIO_DOUT, // I2S 数据输出引脚
            AUDIO_I2S_GPIO_DIN, // I2S 数据输入引脚
            GPIO_NUM_NC, // 功放引脚未使用
            AUDIO_CODEC_ES8311_ADDR, // ES8311 地址
            AUDIO_CODEC_ES7210_ADDR, // ES7210 地址
            AUDIO_INPUT_REFERENCE); // 音频输入参考
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
DECLARE_BOARD(LichuangDevBoard);