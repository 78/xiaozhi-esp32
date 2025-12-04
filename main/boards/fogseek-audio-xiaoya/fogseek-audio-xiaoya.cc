#include "wifi_board.h"
#include "config.h"
#include "power_manager.h"
#include "led_controller.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include "display.h"
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>
#include <esp_log.h>

#define TAG "FogSeekAudioXiaoYa"

class FogSeekAudioXiaoYa : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    FogSeekPowerManager power_manager_;
    FogSeekLedController led_controller_;
    AudioCodec *audio_codec_ = nullptr;

    // 添加自动唤醒标志位
    bool auto_wake_flag_ = false;

    // 初始化电源管理器
    void InitializePowerManager()
    {
        power_pin_config_t power_pin_config = {
            .hold_gpio = PWR_HOLD_GPIO,
            .charging_gpio = PWR_CHARGING_GPIO,
            .charge_done_gpio = PWR_CHARGE_DONE_GPIO,
            .adc_gpio = BATTERY_ADC_GPIO};
        power_manager_.Initialize(&power_pin_config);
    }

    // 初始化LED控制器
    void InitializeLedController()
    {
        led_pin_config_t led_pin_config = {
            .red_gpio = LED_RED_GPIO,
            .green_gpio = LED_GREEN_GPIO};
        led_controller_.InitializeLeds(power_manager_, &led_pin_config);
    }

    // 初始化按键回调
    void InitializeButtonCallbacks()
    {
        ctrl_button_.OnPressDown([this]()
                                 {
                                     led_controller_.SetPrePowerOnState(true); // 按键按下时设置预开机标志位
                                 });
        ctrl_button_.OnPressUp([this]()
                               {
                                   led_controller_.SetPrePowerOnState(false); // 按键松开时清除预开机标志位
                               });

        ctrl_button_.OnClick([this]()
                             {
                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 切换聊天状态（打断）
                             });
        ctrl_button_.OnDoubleClick([this]()
                                   { xTaskCreate([](void *param)
                                                 {
                                            auto* board = static_cast<FogSeekAudio*>(param);
                                            WifiStation::GetInstance().Stop(); 
                                            board->wifi_config_mode_ = true;
                                            board->EnterWifiConfigMode(); // 双击进入WiFi配网
                                            vTaskDelete(nullptr); }, "wifi_config_task", 4096, this, 5, nullptr); });

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
            auto_wake_flag_ = false; // 关闭标志位

            auto &app = Application::GetInstance();
            // USB供电需要播放音效
            if (power_manager_.IsUsbPowered())
                app.PlaySound(Lang::Sounds::OGG_SUCCESS);

            vTaskDelay(pdMS_TO_TICKS(500)); // 添加延时确保声音播放完成
                                            // 进入聆听状态
            app.Schedule([]()
                         {
            auto &app = Application::GetInstance();
            app.ToggleChatState(); });
        }
    }

    // 设备状态变更处理函数
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state)
    {
        // 只有在设备开机状态下才处理LED和显示屏状态
        if (power_manager_.IsPowerOn())
        {
            led_controller_.HandleDeviceState(current_state, power_manager_);

            // 处理自动唤醒逻辑
            HandleAutoWake(current_state);
        }
    }

    // 电源状态变更处理函数，用于关机充电时，充电状态变化更新指示灯
    void OnPowerStateChanged(FogSeekPowerManager::PowerState state)
    {
        if (!power_manager_.IsPowerOn() || Application::GetInstance().GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            led_controller_.UpdateBatteryStatus(power_manager_);
        }
    }

public:
    FogSeekAudioXiaoYa() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializeButtonCallbacks();
        InitializePowerManager();
        InitializeLedController();

        // 设置电源状态变化回调函数
        power_manager_.SetPowerStateCallback([this](FogSeekPowerManager::PowerState state)
                                             { OnPowerStateChanged(state); });

        // 注册设备交互状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state)
                                                                           { OnDeviceStateChanged(previous_state, current_state); });
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    // 重写StartNetwork方法，实现自定义Wi-Fi热点名称
    virtual void StartNetwork() override
    {
        // User can press BOOT button while starting to enter WiFi configuration mode
        if (wifi_config_mode_)
        {
            EnterWifiConfigMode();
            return;
        }

        // If no WiFi SSID is configured, enter WiFi configuration mode
        auto &ssid_manager = SsidManager::GetInstance();
        auto ssid_list = ssid_manager.GetSsidList();
        if (ssid_list.empty())
        {
            wifi_config_mode_ = true;
            EnterWifiConfigMode();
            return;
        }

        // For normal operation, use the parent class's logic
        WifiBoard::StartNetwork();
    }

    // 重新实现EnterWifiConfigMode方法，将热点名称前缀设置为"Xiaoya"
    void EnterWifiConfigMode()
    {
        auto &application = Application::GetInstance();
        application.SetDeviceState(kDeviceStateWifiConfiguring);

        auto &wifi_ap = WifiConfigurationAp::GetInstance();
        wifi_ap.SetLanguage(Lang::CODE);
        wifi_ap.SetSsidPrefix("Xiaoya");
        wifi_ap.Start();

        // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
        std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
        hint += wifi_ap.GetSsid();
        hint += Lang::Strings::ACCESS_VIA_BROWSER;
        hint += wifi_ap.GetWebServerUrl();
        hint += "\n\n";

        // 播报配置 WiFi 的提示
        application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::OGG_WIFICONFIG);

#if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
        auto display = Board::GetInstance().GetDisplay();
        auto codec = Board::GetInstance().GetAudioCodec();
        int channel = 1;
        if (codec)
        {
            channel = codec->input_channels();
        }
        ESP_LOGI(TAG, "Start receiving WiFi credentials from audio, input channels: %d", channel);
        audio_wifi_config::ReceiveWifiCredentialsFromAudio(&application, &wifi_ap, display, channel);
#endif

        // Wait forever until reset after configuration
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }

    ~FogSeekAudioXiaoYa()
    {
    }
};

DECLARE_BOARD(FogSeekAudioXiaoYa);