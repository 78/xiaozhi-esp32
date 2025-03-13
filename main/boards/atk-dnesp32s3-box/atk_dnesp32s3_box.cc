#include "wifi_board.h"
#include "audio_codec.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "i2c_device.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>

#define TAG "atk_dnesp32s3_box" // 日志标签

LV_FONT_DECLARE(font_puhui_20_4); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_20_4); // 声明Font Awesome字体

// XL9555 I2C扩展芯片类
class XL9555 : public I2cDevice {
public:
    XL9555(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x06, 0x1B); // 配置端口0为输出模式
        WriteReg(0x07, 0xFE); // 配置端口1为输出模式
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

// ATK DNESP32S3 BOX开发板类
class atk_dnesp32s3_box : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C总线句柄
    i2c_master_dev_handle_t xl9555_handle_; // XL9555设备句柄
    Button boot_button_; // 启动按钮
    LcdDisplay* display_; // LCD显示屏
    XL9555* xl9555_; // XL9555扩展芯片
    
    // 初始化I2C外设
    void InitializeI2c() {
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0, // I2C端口号
            .sda_io_num = GPIO_NUM_48, // SDA引脚
            .scl_io_num = GPIO_NUM_45, // SCL引脚
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

    // 初始化ATK ST7789 80显示屏
    void InitializeATK_ST7789_80_Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        /* 配置RD引脚 */
        gpio_config_t gpio_init_struct;
        gpio_init_struct.intr_type = GPIO_INTR_DISABLE; // 禁用中断
        gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT; // 输入输出模式
        gpio_init_struct.pin_bit_mask = 1ull << LCD_NUM_RD; // RD引脚
        gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE; // 启用上拉
        gpio_config(&gpio_init_struct); // 配置GPIO
        gpio_set_level(LCD_NUM_RD, 1); // 设置RD引脚为高电平

        esp_lcd_i80_bus_handle_t i80_bus = NULL;
        esp_lcd_i80_bus_config_t bus_config = {
            .dc_gpio_num = LCD_NUM_DC, // DC引脚
            .wr_gpio_num = LCD_NUM_WR, // WR引脚
            .clk_src = LCD_CLK_SRC_DEFAULT, // 时钟源
            .data_gpio_nums = {
                GPIO_LCD_D0, // 数据引脚D0
                GPIO_LCD_D1, // 数据引脚D1
                GPIO_LCD_D2, // 数据引脚D2
                GPIO_LCD_D3, // 数据引脚D3
                GPIO_LCD_D4, // 数据引脚D4
                GPIO_LCD_D5, // 数据引脚D5
                GPIO_LCD_D6, // 数据引脚D6
                GPIO_LCD_D7, // 数据引脚D7
            },
            .bus_width = 8, // 总线宽度
            .max_transfer_bytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), // 最大传输字节数
            .psram_trans_align = 64, // PSRAM传输对齐
            .sram_trans_align = 4, // SRAM传输对齐
        };
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus)); // 创建I80总线

        esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = LCD_NUM_CS, // CS引脚
            .pclk_hz = (10 * 1000 * 1000), // 像素时钟频率
            .trans_queue_depth = 10, // 传输队列深度
            .on_color_trans_done = nullptr, // 颜色传输完成回调
            .user_ctx = nullptr, // 用户上下文
            .lcd_cmd_bits = 8, // 命令位宽
            .lcd_param_bits = 8, // 参数位宽
            .dc_levels = {
                .dc_idle_level = 0, // DC空闲电平
                .dc_cmd_level = 0, // DC命令电平
                .dc_dummy_level = 0, // DC虚拟电平
                .dc_data_level = 1, // DC数据电平
            },
            .flags = {
                .swap_color_bytes = 0, // 不交换颜色字节
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &panel_io)); // 创建I80面板IO

        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_NUM_RST, // 复位引脚
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // RGB元素顺序
            .bits_per_pixel = 16, // 每像素位数
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel)); // 创建ST7789面板

        esp_lcd_panel_reset(panel); // 复位面板
        esp_lcd_panel_init(panel); // 初始化面板
        esp_lcd_panel_invert_color(panel, true); // 反转颜色
        esp_lcd_panel_set_gap(panel, 0, 0); // 设置面板间隙
        uint8_t data0[] = {0x00}; // 数据0
        uint8_t data1[] = {0x65}; // 数据1
        esp_lcd_panel_io_tx_param(panel_io, 0x36, data0, 1); // 发送参数
        esp_lcd_panel_io_tx_param(panel_io, 0x3A, data1, 1); // 发送参数
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY); // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y); // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); // 打开显示

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4, // 文本字体
                                        .icon_font = &font_awesome_20_4, // 图标字体
                                        .emoji_font = font_emoji_64_init(), // 表情字体
                                    });
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

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
    }

public:
    atk_dnesp32s3_box() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化I2C
        InitializeATK_ST7789_80_Display(); // 初始化ATK ST7789 80显示屏
        xl9555_->SetOutputState(5, 1); // 设置XL9555输出状态
        xl9555_->SetOutputState(7, 1); // 设置XL9555输出状态
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static ATK_NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN); // 创建无音频编解码器
        return &audio_codec;
    }
    
    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(atk_dnesp32s3_box); // 声明开发板