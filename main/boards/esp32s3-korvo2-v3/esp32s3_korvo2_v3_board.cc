#include "wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "audio_codecs/box_audio_codec.h"  // 引入Box音频编解码器库，用于音频处理
#include "display/lcd_display.h"  // 引入LCD显示库，提供显示功能
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件

#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "i2c_device.h"  // 引入I2C设备库，用于I2C通信
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备

#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include <esp_lcd_panel_vendor.h>  // 引入LCD面板厂商库，提供LCD面板相关功能
#include <esp_io_expander_tca9554.h>  // 引入TCA9554 IO扩展器库，用于扩展IO口
#include <esp_lcd_ili9341.h>
#include <driver/i2c_master.h>  // 引入I2C主设备库，用于I2C通信
#include <driver/spi_common.h>  // 引入SPI通用库，用于SPI通信
#include <wifi_station.h>  // 引入WiFi站库，用于WiFi连接管理

#define TAG "esp32s3_korvo2_v3"  // 定义日志标签，用于标识日志来源

LV_FONT_DECLARE(font_puhui_20_4);  // 声明普黑字体
LV_FONT_DECLARE(font_awesome_20_4);  // 声明Font Awesome字体

// Init ili9341 by custom cmd
 static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
     {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},
     {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},
     {0xC5, (uint8_t []){0xD0}, 1, 0},
     {0xC1, (uint8_t []){0x02}, 1, 0},
     {0xB4, (uint8_t []){0x02}, 1, 0},
     {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},
     {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},
 
     {0xB1, (uint8_t []){00, 0x1B}, 2, 0},
     {0x36, (uint8_t []){0x08}, 1, 0},
     {0x3A, (uint8_t []){0x55}, 1, 0},
     {0xB7, (uint8_t []){0x06}, 1, 0},
 
     {0x11, (uint8_t []){0}, 0x80, 0},
     {0x29, (uint8_t []){0}, 0x80, 0},
 
     {0, (uint8_t []){0}, 0xff, 0},
 };
 
 

// ESP32-S3 Korvo2 V3板子类，继承自WifiBoard
class Esp32S3Korvo2V3Board : public WifiBoard {
private:
    Button boot_button_;  // 启动按钮对象
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    LcdDisplay* display_;  // LCD显示对象
    esp_io_expander_handle_t io_expander_ = NULL;  // IO扩展器句柄

    // 初始化I2C总线
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,  // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // I2C时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 初始化I2C总线
    }

    // I2C设备检测
    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));  // 探测I2C设备
                if (ret == ESP_OK) {
                    printf("%02x ", address);  // 打印设备地址
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");  // 打印超时标志
                } else {
                    printf("-- ");  // 打印未连接标志
                }
            }
            printf("\r\n");
        }
    }

    // 初始化TCA9554 IO扩展器
    void InitializeTca9554() {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander_);  // 创建TCA9554对象
        if(ret != ESP_OK) {
            ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000, &io_expander_);  // 尝试创建TCA9554A对象
            if(ret != ESP_OK) {
                ESP_LOGE(TAG, "TCA9554 create returned error");  // 记录错误日志
                return;
            }
        }
        // 配置IO0-IO3为输出模式
        ESP_ERROR_CHECK(esp_io_expander_set_dir(io_expander_, 
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | 
            IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, 
            IO_EXPANDER_OUTPUT));

        // 复位LCD和TouchPad
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));  // 设置引脚电平为高
        vTaskDelay(pdMS_TO_TICKS(300));  // 延时300ms
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 0));  // 设置引脚电平为低
        vTaskDelay(pdMS_TO_TICKS(300));  // 延时300ms
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));  // 设置引脚电平为高
    }

    // 启用LCD片选信号
    void EnableLcdCs() {
        if(io_expander_ != NULL) {
            esp_io_expander_set_level(io_expander_, IO_EXPANDER_PIN_NUM_3, 0);  // 置低 LCD CS
        }
    }

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_0;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚（未连接）
        buscfg.sclk_io_num = GPIO_NUM_1;  // SCLK引脚
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

    void InitializeIli9341Display() {
         esp_lcd_panel_io_handle_t panel_io = nullptr;
         esp_lcd_panel_handle_t panel = nullptr;
 
         // 液晶屏控制IO初始化
         ESP_LOGD(TAG, "Install panel IO");
         esp_lcd_panel_io_spi_config_t io_config = {};
         io_config.cs_gpio_num = GPIO_NUM_NC;
         io_config.dc_gpio_num = GPIO_NUM_2;
         io_config.spi_mode = 0;
         io_config.pclk_hz = 40 * 1000 * 1000;
         io_config.trans_queue_depth = 10;
         io_config.lcd_cmd_bits = 8;
         io_config.lcd_param_bits = 8;
         ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));
 
         // 初始化液晶屏驱动芯片
         ESP_LOGD(TAG, "Install LCD driver");
         const ili9341_vendor_config_t vendor_config = {
             .init_cmds = &vendor_specific_init[0],
             .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
         };
 
         esp_lcd_panel_dev_config_t panel_config = {};
         panel_config.reset_gpio_num = GPIO_NUM_NC;
         // panel_config.flags.reset_active_high = 0,
         panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
         panel_config.bits_per_pixel = 16;
         panel_config.vendor_config = (void *)&vendor_config;
         ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
         
         ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
         EnableLcdCs();
         ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
         ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
         ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
         ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
         ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
         display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                     {
                                         .text_font = &font_puhui_20_4,
                                         .icon_font = &font_awesome_20_4,
                                         .emoji_font = font_emoji_64_init(),
                                     });
     }

    // 初始化ST7789显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;  // CS引脚（未连接）
        io_config.dc_gpio_num = GPIO_NUM_2;  // DC引脚
        io_config.spi_mode = 0;  // SPI模式
        io_config.pclk_hz = 60 * 1000 * 1000;  // 时钟频率
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
        EnableLcdCs();  // 启用LCD片选信号
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
    }

public:
    // 构造函数
    Esp32S3Korvo2V3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "Initializing esp32s3_korvo2_v3 Board");  // 记录信息日志
        InitializeI2c();  // 初始化I2C总线
        I2cDetect();  // 检测I2C设备
        InitializeTca9554();  // 初始化TCA9554 IO扩展器
        InitializeSpi();  // 初始化SPI总线
        InitializeButtons();  // 初始化按钮
        InitializeSt7789Display();  // 初始化ST7789显示屏
        #ifdef LCD_TYPE_ILI9341_SERIAL
         InitializeIli9341Display(); 
         #else
         InitializeSt7789Display(); 
         #endif
        InitializeIot();  // 初始化物联网设备
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);  // 创建Box音频编解码器对象
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display *GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(Esp32S3Korvo2V3Board);  // 声明ESP32-S3 Korvo2 V3板子