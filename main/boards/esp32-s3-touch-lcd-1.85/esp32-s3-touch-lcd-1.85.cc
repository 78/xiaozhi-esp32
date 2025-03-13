#include "wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "audio_codecs/no_audio_codec.h"  // 引入无音频编解码器库，用于无音频处理
#include "display/lcd_display.h"  // 引入LCD显示库，提供显示功能
#include "system_reset.h"  // 引入系统重置库，用于系统重置功能
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件
#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备

#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include "i2c_device.h"  // 引入I2C设备库，用于I2C通信
#include <driver/i2c.h>  // 引入I2C驱动库，用于I2C通信
#include <driver/ledc.h>  // 引入LEDC驱动库，用于PWM控制
#include <wifi_station.h>  // 引入WiFi站库，用于WiFi连接管理
#include <esp_lcd_panel_io.h>  // 引入LCD面板IO库，用于LCD面板通信
#include <esp_lcd_panel_ops.h>  // 引入LCD面板操作库，用于LCD面板操作
#include <esp_lcd_st77916.h>  // 引入ST77916 LCD驱动库，用于控制ST77916显示屏
#include <esp_timer.h>  // 引入ESP定时器库，用于定时任务
#include "esp_io_expander_tca9554.h"  // 引入TCA9554 IO扩展器库，用于扩展IO口

#define TAG "waveshare_lcd_1_85"  // 定义日志标签，用于标识日志来源

LV_FONT_DECLARE(font_puhui_16_4);  // 声明普黑字体
LV_FONT_DECLARE(font_awesome_16_4);  // 声明Font Awesome字体

// 自定义板子类，继承自WifiBoard
class CustomBoard : public WifiBoard {
private:
    Button boot_button_;  // 启动按钮对象
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    esp_io_expander_handle_t io_expander = NULL;  // IO扩展器句柄
    LcdDisplay* display_;  // LCD显示对象

    // 初始化I2C总线
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,  // I2C端口号
            .sda_io_num = I2C_SDA_IO,  // SDA引脚
            .scl_io_num = I2C_SCL_IO,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // I2C时钟源
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 初始化I2C总线
    }
    
    // 初始化TCA9554 IO扩展器
    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);  // 创建TCA9554对象
        if(ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");  // 记录错误日志

        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT);  // 设置引脚 EXIO0 和 EXIO1 模式为输出
        ESP_ERROR_CHECK(ret);
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);  // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));  // 延时300ms
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 0);  // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));  // 延时300ms
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);  // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
    }

    // 初始化SPI总线
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");  // 记录信息日志

        const spi_bus_config_t bus_config = TAIJIPI_ST77916_PANEL_BUS_QSPI_CONFIG(QSPI_PIN_NUM_LCD_PCLK,
                                                                        QSPI_PIN_NUM_LCD_DATA0,
                                                                        QSPI_PIN_NUM_LCD_DATA1,
                                                                        QSPI_PIN_NUM_LCD_DATA2,
                                                                        QSPI_PIN_NUM_LCD_DATA3,
                                                                        QSPI_LCD_H_RES * 80 * sizeof(uint16_t));  // 配置SPI总线
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化ST77916显示屏
    void Initializest77916Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install panel IO");  // 记录信息日志
        
        const esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(QSPI_PIN_NUM_LCD_CS, NULL, NULL);  // 配置面板IO
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST, &io_config, &panel_io));  // 创建面板IO

        ESP_LOGI(TAG, "Install ST77916 panel driver");  // 记录信息日志
        
        st77916_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,  // 使用QSPI接口
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = QSPI_PIN_NUM_LCD_RST,  // 重置引脚
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // RGB元素顺序
            .bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL,  // 每像素位数
            .vendor_config = &vendor_config,  // 厂商配置
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(panel_io, &panel_config, &panel));  // 创建ST77916面板

        esp_lcd_panel_reset(panel);  // 重置面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,  // 设置文本字体
                                        .icon_font = &font_awesome_16_4,  // 设置图标字体
                                        .emoji_font = font_emoji_64_init(),  // 设置表情字体
                                    });
    }
 
    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
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
    CustomBoard() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();  // 初始化I2C总线
        InitializeTca9554();  // 初始化TCA9554 IO扩展器
        InitializeSpi();  // 初始化SPI总线
        Initializest77916Display();  // 初始化ST77916显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_BOTH, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT);  // 创建无音频编解码器对象

        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建PWM背光对象
        return &backlight;
    }
};

DECLARE_BOARD(CustomBoard);  // 声明自定义板子