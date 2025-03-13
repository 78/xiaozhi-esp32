#include "wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "display/lcd_display.h"  // 引入LCD显示库，提供显示功能
#include "esp_lcd_sh8601.h"  // 引入SH8601 LCD驱动库，用于控制SH8601显示屏
#include "font_awesome_symbols.h"  // 引入Font Awesome字体符号库，用于显示图标

#include "audio_codecs/es8311_audio_codec.h"  // 引入ES8311音频编解码器库，用于音频处理
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件
#include "led/single_led.h"  // 引入单LED库，用于控制LED
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备
#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "axp2101.h"  // 引入AXP2101电源管理库，用于电源管理
#include "i2c_device.h"  // 引入I2C设备库，用于I2C通信
#include <wifi_station.h>  // 引入WiFi站库，用于WiFi连接管理

#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include <esp_lcd_panel_vendor.h>  // 引入LCD面板厂商库，提供LCD面板相关功能
#include <driver/i2c_master.h>  // 引入I2C主设备库，用于I2C通信
#include <driver/spi_master.h>  // 引入SPI主设备库，用于SPI通信
#include "esp_io_expander_tca9554.h"  // 引入TCA9554 IO扩展器库，用于扩展IO口
#include "settings.h"  // 引入设置库，用于管理设备设置

#define TAG "waveshare_amoled_1_8"  // 定义日志标签，用于标识日志来源

LV_FONT_DECLARE(font_puhui_30_4);  // 声明普黑字体
LV_FONT_DECLARE(font_awesome_30_4);  // 声明Font Awesome字体

#define LCD_OPCODE_WRITE_CMD (0x02ULL)  // 定义LCD写命令操作码
#define LCD_OPCODE_READ_CMD (0x03ULL)  // 定义LCD读命令操作码
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)  // 定义LCD写颜色操作码

// SH8601 LCD初始化命令列表
static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},  // 初始化命令1
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},  // 初始化命令2
    {0x35, (uint8_t[]){0x00}, 1, 0},  // 初始化命令3
    {0x53, (uint8_t[]){0x20}, 1, 10},  // 初始化命令4
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},  // 初始化命令5
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},  // 初始化命令6
    {0x51, (uint8_t[]){0x00}, 1, 10},  // 初始化命令7
    {0x29, (uint8_t[]){0x00}, 0, 10},  // 初始化命令8
    {0x51, (uint8_t[]){0xFF}, 1, 0},  // 初始化命令9
};

// 自定义LCD显示类，继承自SpiLcdDisplay
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_30_4,  // 设置文本字体
                        .icon_font = &font_awesome_30_4,  // 设置图标字体
                        .emoji_font = font_emoji_64_init(),  // 设置表情字体
                    }) {
        DisplayLockGuard lock(this);  // 锁定显示
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.1, 0);  // 设置状态栏左边距
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);  // 设置状态栏右边距
    }
};

// 自定义背光类，继承自Backlight
class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;  // LCD面板IO句柄

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100)};  // 计算亮度值
        int lcd_cmd = 0x51;  // LCD命令
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;  // 设置写命令操作码
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));  // 发送亮度参数
    }
};

// Waveshare AMOLED 1.8英寸板子类，继承自WifiBoard
class waveshare_amoled_1_8 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C总线句柄
    Axp2101* axp2101_ = nullptr;  // AXP2101电源管理对象
    esp_timer_handle_t power_save_timer_ = nullptr;  // 电源节省定时器句柄

    Button boot_button_;  // 启动按钮对象
    CustomLcdDisplay* display_;  // 自定义LCD显示对象
    CustomBacklight* backlight_;  // 自定义背光对象
    esp_io_expander_handle_t io_expander = NULL;  // IO扩展器句柄

    // 初始化音频编解码器I2C总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化I2C总线
    }

    // 初始化TCA9554 IO扩展器
    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(codec_i2c_bus_, I2C_ADDRESS, &io_expander);  // 创建TCA9554对象
        if(ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");  // 记录错误日志
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);  // 设置IO方向
        ret |= esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);  // 设置IO方向
        ESP_ERROR_CHECK(ret);  // 检查错误
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);  // 设置IO电平
        ESP_ERROR_CHECK(ret);  // 检查错误
        vTaskDelay(pdMS_TO_TICKS(100));  // 延时100ms
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 0);  // 设置IO电平
        ESP_ERROR_CHECK(ret);  // 检查错误
        vTaskDelay(pdMS_TO_TICKS(300));  // 延时300ms
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);  // 设置IO电平
        ESP_ERROR_CHECK(ret);  // 检查错误
    }

    // 初始化AXP2101电源管理
    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");  // 记录信息日志
        axp2101_ = new Axp2101(codec_i2c_bus_, 0x34);  // 创建AXP2101对象
    }

    // 初始化电源节省定时器
    void InitializePowerSaveTimer() {
        esp_timer_create_args_t power_save_timer_args = {
            .callback = [](void *arg) {
                auto board = static_cast<waveshare_amoled_1_8*>(arg);
                board->PowerSaveCheck();  // 电源节省检查
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "Power Save Timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&power_save_timer_args, &power_save_timer_));  // 创建定时器
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));  // 启动定时器
    }

    // 电源节省检查
    void PowerSaveCheck() {
        const int seconds_to_shutdown = 600;  // 关机时间阈值
        static int seconds = 0;  // 计时器
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {  // 检查设备状态
            seconds = 0;
            return;
        }
        if (!axp2101_->IsDischarging()) {  // 检查是否在放电
            seconds = 0;
            return;
        }

        seconds++;  // 计时器增加
        if (seconds >= seconds_to_shutdown) {  // 检查是否达到关机时间
            axp2101_->PowerOff();  // 关机
        }
    }

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = GPIO_NUM_11;
        buscfg.data0_io_num = GPIO_NUM_4;
        buscfg.data1_io_num = GPIO_NUM_5;
        buscfg.data2_io_num = GPIO_NUM_6;
        buscfg.data3_io_num = GPIO_NUM_7;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
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

    // 初始化SH8601显示屏
    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");  // 记录调试日志
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            EXAMPLE_PIN_NUM_LCD_CS,
            nullptr,
            nullptr
        );
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));  // 创建面板IO

        ESP_LOGD(TAG, "Install LCD driver");  // 记录调试日志
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags ={
                .use_qspi_interface = 1,
            }
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.flags.reset_active_high = 1,
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));  // 创建SH8601面板

        esp_lcd_panel_reset(panel);  // 重置面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, false);  // 反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        display_ = new CustomLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);  // 创建自定义LCD显示对象
        backlight_ = new CustomBacklight(panel_io);  // 创建自定义背光对象
        backlight_->RestoreBrightness();  // 恢复背光亮度
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("BoardControl"));  // 添加板控制设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
        thing_manager.AddThing(iot::CreateThing("Battery"));  // 添加电池设备
    }

public:
    // 构造函数
    waveshare_amoled_1_8() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();  // 初始化音频编解码器I2C总线
        InitializeTca9554();  // 初始化TCA9554 IO扩展器
        InitializeAxp2101();  // 初始化AXP2101电源管理
        InitializeSpi();  // 初始化SPI总线
        InitializeSH8601Display();  // 初始化SH8601显示屏
        InitializeButtons();  // 初始化按钮
        InitializePowerSaveTimer();  // 初始化电源节省定时器
        InitializeIot();  // 初始化物联网设备
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建ES8311音频编解码器对象
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取电池电量和充电状态
    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;
        level = axp2101_->GetBatteryLevel();  // 获取电池电量
        charging = axp2101_->IsCharging();  // 获取充电状态
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);  // 记录电池电量和充电状态
        }
        return true;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        return backlight_;
    }
};

DECLARE_BOARD(waveshare_amoled_1_8);  // 声明Waveshare AMOLED 1.8英寸板子