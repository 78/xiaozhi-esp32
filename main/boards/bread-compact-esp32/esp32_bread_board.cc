#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "display/ssd1306_display.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "ESP32-MarsbearSupport" // 日志标签

LV_FONT_DECLARE(font_puhui_14_1); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_14_1); // 声明Font Awesome字体

// CompactWifiBoard开发板类
class CompactWifiBoard : public WifiBoard {
private:
    Button boot_button_; // 启动按钮
    Button touch_button_; // 触摸按钮
    Button asr_button_; // ASR按钮

    i2c_master_bus_handle_t display_i2c_bus_; // 显示屏I2C总线句柄

    // 初始化显示屏I2C外设
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0, // I2C端口号
            .sda_io_num = DISPLAY_SDA_PIN, // SDA引脚
            .scl_io_num = DISPLAY_SCL_PIN, // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_)); // 创建I2C总线
    }

    // 初始化按钮
    void InitializeButtons() {
        
        // 配置GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // 设置需要配置的GPIO引脚
            .mode = GPIO_MODE_OUTPUT,           // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE      // 禁用中断
        };
        gpio_config(&io_conf);  // 应用配置

        // 启动按钮点击事件
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1); // 设置LED为高电平
            app.ToggleChatState(); // 切换聊天状态
        });

        // ASR按钮点击事件
        asr_button_.OnClick([this]() {
            std::string wake_word="你好小智"; // 唤醒词
            Application::GetInstance().WakeWordInvoke(wake_word); // 唤醒词触发
        });

        // 触摸按钮按下事件
        touch_button_.OnPressDown([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 1); // 设置LED为高电平
            Application::GetInstance().StartListening(); // 开始监听
        });

        // 触摸按钮释放事件
        touch_button_.OnPressUp([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 0); // 设置LED为低电平
            Application::GetInstance().StopListening(); // 停止监听
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Lamp")); // 添加灯设备
    }

public:
    // 构造函数
    CompactWifiBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeDisplayI2c(); // 初始化显示屏I2C
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override 
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        // 单工模式音频编解码器
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        // 双工模式音频编解码器
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1); // 创建SSD1306显示屏对象
        return &display;
    }
};

DECLARE_BOARD(CompactWifiBoard); // 声明开发板