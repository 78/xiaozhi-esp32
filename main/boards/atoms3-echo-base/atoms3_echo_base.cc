#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#define TAG "AtomS3+EchoBase" // 日志标签

LV_FONT_DECLARE(font_puhui_16_4); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_16_4); // 声明Font Awesome字体

// GC9107 LCD初始化命令
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0}, // 初始化命令
    {0xef, (uint8_t[]){0x00}, 0, 0}, // 初始化命令
    {0xb0, (uint8_t[]){0xc0}, 1, 0}, // 初始化命令
    {0xb2, (uint8_t[]){0x2f}, 1, 0}, // 初始化命令
    {0xb3, (uint8_t[]){0x03}, 1, 0}, // 初始化命令
    {0xb6, (uint8_t[]){0x19}, 1, 0}, // 初始化命令
    {0xb7, (uint8_t[]){0x01}, 1, 0}, // 初始化命令
    {0xac, (uint8_t[]){0xcb}, 1, 0}, // 初始化命令
    {0xab, (uint8_t[]){0x0e}, 1, 0}, // 初始化命令
    {0xb4, (uint8_t[]){0x04}, 1, 0}, // 初始化命令
    {0xa8, (uint8_t[]){0x19}, 1, 0}, // 初始化命令
    {0xb8, (uint8_t[]){0x08}, 1, 0}, // 初始化命令
    {0xe8, (uint8_t[]){0x24}, 1, 0}, // 初始化命令
    {0xe9, (uint8_t[]){0x48}, 1, 0}, // 初始化命令
    {0xea, (uint8_t[]){0x22}, 1, 0}, // 初始化命令
    {0xc6, (uint8_t[]){0x30}, 1, 0}, // 初始化命令
    {0xc7, (uint8_t[]){0x18}, 1, 0}, // 初始化命令
    {0xf0,
    (uint8_t[]){0x1f, 0x28, 0x04, 0x3e, 0x2a, 0x2e, 0x20, 0x00, 0x0c, 0x06,
                0x00, 0x1c, 0x1f, 0x0f},
    14, 0}, // 初始化命令
    {0xf1,
    (uint8_t[]){0x00, 0x2d, 0x2f, 0x3c, 0x6f, 0x1c, 0x0b, 0x00, 0x00, 0x00,
                0x07, 0x0d, 0x11, 0x0f},
    14, 0}, // 初始化命令
};

// AtomS3EchoBaseBoard开发板类
class AtomS3EchoBaseBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C总线句柄
    Display* display_; // 显示屏对象
    Button boot_button_; // 启动按钮
    bool is_echo_base_connected_ = false; // Echo Base是否连接的标志

    // 初始化I2C外设
    void InitializeI2c() {
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1, // I2C端口号
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
    }

    // I2C设备检测
    void I2cDetect() {
        is_echo_base_connected_ = false;
        uint8_t echo_base_connected_flag = 0x00; // Echo Base连接标志
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200)); // 探测设备
                if (ret == ESP_OK) {
                    printf("%02x ", address); // 设备存在
                    if (address == 0x18) {
                        echo_base_connected_flag |= 0xF0; // 设置Echo Base连接标志
                    } else if (address == 0x43) {
                        echo_base_connected_flag |= 0x0F; // 设置Echo Base连接标志
                    }
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU "); // 超时
                } else {
                    printf("-- "); // 设备不存在
                }
            }
            printf("\r\n");
        }
        is_echo_base_connected_ = (echo_base_connected_flag == 0xFF); // 判断Echo Base是否连接
    }

    // 检查Echo Base连接状态
    void CheckEchoBaseConnection() {
        if (is_echo_base_connected_) {
            return; // 如果已连接，直接返回
        }
        
        // 弹出错误页面
        InitializeSpi();
        InitializeGc9107Display();
        InitializeButtons();
        display_->SetStatus(Lang::Strings::ERROR); // 设置状态为错误
        display_->SetEmotion("sad"); // 设置表情为悲伤
        display_->SetChatMessage("system", "Echo Base\nnot connected"); // 显示错误信息
        
        while (1) {
            ESP_LOGE(TAG, "Atomic Echo Base is disconnected"); // 日志：Echo Base未连接
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟1秒

            // 重新检测
            I2cDetect();
            if (is_echo_base_connected_) {
                vTaskDelay(pdMS_TO_TICKS(500)); // 延迟500毫秒
                I2cDetect();
                if (is_echo_base_connected_) {
                    ESP_LOGI(TAG, "Atomic Echo Base is reconnected"); // 日志：Echo Base重新连接
                    vTaskDelay(pdMS_TO_TICKS(200)); // 延迟200毫秒
                    esp_restart(); // 重启设备
                }
            }
        }
    }

    // 初始化SPI外设
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus"); // 日志：初始化SPI总线
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_21; // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO引脚未使用
        buscfg.sclk_io_num = GPIO_NUM_17; // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO)); // 初始化SPI总线
    }

    // 初始化GC9107显示屏
    void InitializeGc9107Display() {
        ESP_LOGI(TAG, "Init GC9107 display"); // 日志：初始化GC9107显示屏

        ESP_LOGI(TAG, "Install panel IO"); // 日志：安装面板IO
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_15; // CS引脚
        io_config.dc_gpio_num = GPIO_NUM_33; // DC引脚
        io_config.spi_mode = 0; // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000; // 像素时钟频率
        io_config.trans_queue_depth = 10; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // 命令位宽
        io_config.lcd_param_bits = 8; // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle)); // 创建SPI面板IO
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver"); // 日志：安装GC9A01面板驱动
        esp_lcd_panel_handle_t panel_handle = NULL;
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds, // 初始化命令
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t), // 初始化命令大小
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_34; // 复位引脚
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR; // RGB字节序
        panel_config.bits_per_pixel = 16; // 每像素位数
        panel_config.vendor_config = &gc9107_vendor_config; // 供应商配置

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle)); // 创建GC9A01面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); // 复位面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // 打开显示

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
            {
                .text_font = &font_puhui_16_4, // 文本字体
                .icon_font = &font_awesome_16_4, // 图标字体
                .emoji_font = font_emoji_32_init(), // 表情字体
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
    }

public:
    AtomS3EchoBaseBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化I2C
        I2cDetect(); // I2C设备检测
        CheckEchoBaseConnection(); // 检查Echo Base连接状态
        InitializeSpi(); // 初始化SPI
        InitializeGc9107Display(); // 初始化GC9107显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
        GetBacklight()->RestoreBrightness(); // 恢复背光亮度
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_1, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, 
            AUDIO_CODEC_ES8311_ADDR, 
            false); // 创建ES8311音频编解码器
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT); // 创建PWM背光对象
        return &backlight;
    }
};

DECLARE_BOARD(AtomS3EchoBaseBoard); // 声明开发板