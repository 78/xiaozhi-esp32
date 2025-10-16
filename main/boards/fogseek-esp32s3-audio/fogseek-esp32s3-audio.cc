#include "wifi_board.h"
#include "config.h"
#include "power_manager.h"
#include "led_controller.h"
#include "codecs/es8311_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#include "codecs/no_audio_codec.h"

#define TAG "FogSeekEsp32s3Audio"

class FogSeekEsp32s3Audio : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    PowerManager power_manager_;
    LedController led_controller_;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    AudioCodec *audio_codec_ = nullptr;

    // 添加自动唤醒标志位
    bool auto_wake_flag_ = false;

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

        // // 开机自动唤醒
        // auto_wake_flag_ = true;
        // OnDeviceStateChanged(DeviceState::kDeviceStateUnknown,
        //                      Application::GetInstance().GetDeviceState());

        ESP_LOGI(TAG, "Device powered on.");
    }

    // 关机流程
    void PowerOff()
    {
        power_manager_.PowerOff();
        led_controller_.SetPowerState(false);
        led_controller_.UpdateBatteryStatus(power_manager_);

        // // 重置自动唤醒标志位到默认状态
        // auto_wake_flag_ = false;
        // Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle);

        ESP_LOGI(TAG, "Device powered off.");
    }

    // 处理自动唤醒逻辑
    // void HandleAutoWake(DeviceState current_state)
    // {
    //     // 检查是否需要自动唤醒
    //     if (auto_wake_flag_ && current_state == DeviceState::kDeviceStateIdle)
    //     {
    //         auto &app = Application::GetInstance();
    //         app.WakeWordInvoke("你好小智"); // 自动唤醒
    //         auto_wake_flag_ = false;        // 关闭标志位
    //         ESP_LOGI(TAG, "Auto wake word invoked after initialization");
    //     }
    // }

    // 设备状态变更处理函数
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state)
    {
        // 只有在设备开机状态下才处理LED和显示屏状态
        if (power_manager_.IsPowerOn())
        {
            led_controller_.HandleDeviceState(current_state, power_manager_);

            // // 处理自动唤醒逻辑
            // HandleAutoWake(current_state);
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
    FogSeekEsp32s3Audio() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        power_manager_.Initialize();
        led_controller_.InitializeLeds(power_manager_);
        InitializeButtonCallbacks();

        // 设置电源状态变化回调函数
        power_manager_.SetPowerStateCallback([this](PowerManager::PowerState state)
                                             { OnPowerStateChanged(state); });

        // 注册设备状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state)
                                                                           { OnDeviceStateChanged(previous_state, current_state); });
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    ~FogSeekEsp32s3Audio()
    {
    }
};

DECLARE_BOARD(FogSeekEsp32s3Audio);