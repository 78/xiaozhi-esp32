#include "wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "audio_codecs/es8311_audio_codec.h"  // 引入ES8311音频编解码器库，用于音频处理
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件
#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备
#include "led/circular_strip.h"  // 引入环形LED灯带库，用于控制LED

#include <wifi_station.h>  // 引入WiFi站库，用于WiFi连接管理
#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include <esp_efuse_table.h>  // 引入eFuse表库，用于配置eFuse
#include <driver/i2c_master.h>  // 引入I2C主设备库，用于I2C通信

#define TAG "KevinBoxBoard"  // 定义日志标签，用于标识日志来源

// KevinBox板子类，继承自WifiBoard
class KevinBoxBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // 音频编解码器I2C总线句柄
    Button boot_button_;  // 启动按钮对象

    // 初始化音频编解码器I2C总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,  // I2C端口号
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化I2C总线
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

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
    }

public:
    // 构造函数
    KevinBoxBoard() : boot_button_(BOOT_BUTTON_GPIO) {  
        // 将ESP32C3的VDD SPI引脚配置为普通GPIO口
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);  // 设置eFuse位
        
        InitializeCodecI2c();  // 初始化音频编解码器I2C总线
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, 8);  // 创建环形LED灯带对象
        return &led;
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建ES8311音频编解码器对象
        return &audio_codec;
    }
};

DECLARE_BOARD(KevinBoxBoard);  // 声明KevinBox板子