#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/ssd1306_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "CompactWifiBoard" // 日志标签

LV_FONT_DECLARE(font_puhui_14_1); // 声明普黑字体
LV_FONT_DECLARE(font_awesome_14_1); // 声明Font Awesome字体

// CompactWifiBoard开发板类
class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_; // 显示屏I2C总线句柄
    Button boot_button_; // 启动按钮
    Button touch_button_; // 触摸按钮
    Button volume_up_button_; // 音量增加按钮
    Button volume_down_button_; // 音量减少按钮

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
        // 启动按钮点击事件
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            }
            app.ToggleChatState(); // 切换聊天状态
        });

        // 触摸按钮按下事件
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening(); // 开始监听
        });

        // 触摸按钮释放事件
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening(); // 停止监听
        });

        // 音量增加按钮点击事件
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10; // 音量增加10
            if (volume > 100) {
                volume = 100; // 音量最大为100
            }
            codec->SetOutputVolume(volume); // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); // 显示音量通知
        });

        // 音量增加按钮长按事件
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100); // 设置音量为最大值
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME); // 显示最大音量通知
        });

        // 音量减少按钮点击事件
        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10; // 音量减少10
            if (volume < 0) {
                volume = 0; // 音量最小为0
            }
            codec->SetOutputVolume(volume); // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); // 显示音量通知
        });

        // 音量减少按钮长按事件
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0); // 设置音量为0（静音）
            GetDisplay()->ShowNotification(Lang::Strings::MUTED); // 显示静音通知
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
    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO), // 初始化启动按钮
        touch_button_(TOUCH_BUTTON_GPIO), // 初始化触摸按钮
        volume_up_button_(VOLUME_UP_BUTTON_GPIO), // 初始化音量增加按钮
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) { // 初始化音量减少按钮
        InitializeDisplayI2c(); // 初始化显示屏I2C
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO); // 创建单LED对象
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
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