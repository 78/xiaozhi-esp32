#include "wifi_board.h"
#include "config.h"
#include "power_manager.h"
#include "display_manager.h"
#include "led_controller.h"
#include "codecs/es8311_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#define TAG "FogSeekEsp32s3EdgeLcd15"

class FogSeekEsp32s3EdgeLcd15 : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    PowerManager power_manager_;
    DisplayManager display_manager_;
    LedController led_controller_;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    AudioCodec *audio_codec_ = nullptr;

    // 添加自动唤醒标志位
    bool auto_wake_flag_ = false;

    // 初始化I2C外设
    void InitializeI2c()
    {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    // // 初始化音频功放引脚并默认关闭功放
    // void InitializeAudioAmplifier()
    // {
    //     gpio_config_t io_conf;
    //     io_conf.intr_type = GPIO_INTR_DISABLE;
    //     io_conf.mode = GPIO_MODE_OUTPUT;
    //     io_conf.pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN);
    //     io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //     io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //     gpio_config(&io_conf);
    //     SetAudioAmplifierState(false); // 默认关闭功放
    // }

    // // 设置音频功放状态
    // void SetAudioAmplifierState(bool enable)
    // {
    //     gpio_set_level(AUDIO_CODEC_PA_PIN, enable ? 1 : 0);
    // }

    // 初始化按键回调
    void InitializeButtonCallbacks()
    {
        ctrl_button_.OnClick([this]()
                             {
                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 切换聊天状态（打断）
                             });

        ctrl_button_.OnLongPress([this]()
                                 {
            // 切换电源状态
            if (!power_manager_.IsPowerOn()) {
                PowerOn();
            } else {
                PowerOff();
            } });
    }

    // 开机流程
    void PowerOn()
    {
        power_manager_.PowerOn();
        led_controller_.SetPowerState(true);
        led_controller_.UpdateBatteryStatus(power_manager_);
        // display_manager_.RestoreBrightness();
        // SetAudioAmplifierState(true);

        // 开机自动唤醒
        auto_wake_flag_ = true;
        OnDeviceStateChanged(DeviceState::kDeviceStateUnknown,
                             Application::GetInstance().GetDeviceState());

        ESP_LOGI(TAG, "Device powered on.");
    }

    // 关机流程
    void PowerOff()
    {
        power_manager_.PowerOff();
        led_controller_.SetPowerState(false);
        led_controller_.UpdateBatteryStatus(power_manager_);
        // display_manager_.SetBrightness(0);
        // SetAudioAmplifierState(false);

        // 重置自动唤醒标志位到默认状态
        auto_wake_flag_ = false;
        Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle);

        ESP_LOGI(TAG, "Device powered off.");
    }

    // 处理自动唤醒逻辑
    void HandleAutoWake(DeviceState current_state)
    {
        // 检查是否需要自动唤醒
        if (auto_wake_flag_ && current_state == DeviceState::kDeviceStateIdle)
        {
            auto &app = Application::GetInstance();
            app.WakeWordInvoke("你好小智"); // 自动唤醒
            auto_wake_flag_ = false;        // 关闭标志位
            ESP_LOGI(TAG, "Auto wake word invoked after initialization");
        }
    }

    // 设备状态变更处理函数
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state)
    {
        // 只有在设备开机状态下才处理LED和显示屏状态
        if (power_manager_.IsPowerOn())
        {
            led_controller_.HandleDeviceState(current_state, power_manager_);
            // display_manager_.HandleDeviceState(current_state);

            // 处理自动唤醒逻辑
            HandleAutoWake(current_state);
        }
    }

    // 电源状态变更处理函数
    void OnPowerStateChanged(PowerManager::PowerState state)
    {
        if (!power_manager_.IsPowerOn() ||
            Application::GetInstance().GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            led_controller_.UpdateBatteryStatus(power_manager_);
        }
    }

public:
    FogSeekEsp32s3EdgeLcd15() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializeI2c();
        // InitializeAudioAmplifier();
        power_manager_.Initialize();
        led_controller_.InitializeLeds(power_manager_);
        // display_manager_.Initialize();
        InitializeButtonCallbacks();

        // 设置电源状态变化回调函数
        power_manager_.SetPowerStateCallback([this](PowerManager::PowerState state)
                                             { OnPowerStateChanged(state); });

        // 注册设备状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state)
                                                                           { OnDeviceStateChanged(previous_state, current_state); });
    }

    // virtual Display *GetDisplay() override
    // {
    //     return display_manager_.GetDisplay();
    // }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            (i2c_port_t)0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            true,
            false);
        return &audio_codec;
    }

    ~FogSeekEsp32s3EdgeLcd15()
    {
        if (i2c_bus_)
        {
            i2c_del_master_bus(i2c_bus_);
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3EdgeLcd15);