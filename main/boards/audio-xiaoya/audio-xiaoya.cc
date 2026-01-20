#include "wifi_board.h"
#include "config.h"
#include "power_manager.h"
#include "led_controller.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "mcp_tools.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include "device_state_machine.h"
#include <esp_log.h>
#include <driver/rtc_io.h>
#include <wifi_manager.h>

#define TAG "AudioXiaoYa"

class AudioXiaoYa : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    FogSeekPowerManager power_manager_;
    FogSeekLedController led_controller_;
    AudioCodec *audio_codec_ = nullptr;
    esp_timer_handle_t check_idle_timer_ = nullptr;
    bool is_intercom_mode_active_ = false;

    void InitializePowerManager()
    {
        power_pin_config_t power_pin_config = {
            .hold_gpio = PWR_HOLD_GPIO,
            .charging_gpio = PWR_CHARGING_GPIO,
            .charge_done_gpio = PWR_CHARGE_DONE_GPIO,
            .adc_gpio = BATTERY_ADC_GPIO};
        power_manager_.Initialize(&power_pin_config);
    }

    void InitializeLedController()
    {
        led_pin_config_t led_pin_config = {
            .red_gpio = LED_RED_GPIO,
            .green_gpio = LED_GREEN_GPIO};
        led_controller_.InitializeLeds(power_manager_, &led_pin_config);
    }

    void InitializeAudioOutputControl()
    {
        auto codec = GetAudioCodec();
        codec->SetOutputVolume(0);
    }

    void InitializeButtonCallbacks()
    {
        // 对讲模式：按下开始录音，松开结束录音
        ctrl_button_.OnPressDown([this]()
                                 {
            auto &app = Application::GetInstance();
            
            is_intercom_mode_active_ = true;
            app.GetAudioService().EnableVoiceProcessing(false);
            
            if (app.GetDeviceState() != DeviceState::kDeviceStateListening) {
                app.StartListening();
            }
            
            ESP_LOGI(TAG, "Intercom mode started - button pressed down, VAD disabled"); });

        ctrl_button_.OnPressUp([this]()
                               {
            if (is_intercom_mode_active_) {
                auto &app = Application::GetInstance();
                
                is_intercom_mode_active_ = false;
                
                if (app.GetDeviceState() == DeviceState::kDeviceStateListening) {
                    app.StopListening();
                }
                
                app.GetAudioService().EnableVoiceProcessing(true);
                ESP_LOGI(TAG, "Intercom mode ended - button released, VAD enabled");
            } });

        // 单击：切换聊天状态
        ctrl_button_.OnClick([this]()
                             {
                                 if (is_intercom_mode_active_) return;

                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); });

        // 双击：进入WiFi配置模式
        ctrl_button_.OnDoubleClick([this]()
                                   {
            if (is_intercom_mode_active_) return;
            
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == DeviceState::kDeviceStateStarting)
            {
                EnterWifiConfigMode();
                return;
            } });

        // 三击：关机
        ctrl_button_.OnMultipleClick([this]()
                                     {
            auto &app = Application::GetInstance();
            if (is_intercom_mode_active_) return;
            
            ESP_LOGI(TAG, "Triple click detected, powering off device");
            app.Alert("INFO", "关机中...", "neutral", "");
            PowerOff(); }, 3);
    }

    void HandleAutoWake()
    {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            app.Schedule([]()
                         {
                            auto &app = Application::GetInstance();
                            app.ToggleChatState(); });
        }
        else
        {
            esp_timer_handle_t check_timer;
            esp_timer_create_args_t timer_args = {};
            timer_args.callback = [](void *arg)
            {
                auto instance = static_cast<AudioXiaoYa *>(arg);
                instance->HandleAutoWake();
            };
            timer_args.arg = this;
            timer_args.name = "check_idle_timer";
            esp_timer_create(&timer_args, &check_timer);
            esp_timer_start_once(check_timer, 500000);
        }
    }

    void PowerOn()
    {
        power_manager_.PowerOn();
        led_controller_.UpdateLedStatus(power_manager_);

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(70);

        ESP_LOGI(TAG, "Device powered on.");

        HandleAutoWake();
    }

    void PowerOff()
    {
        power_manager_.PowerOff();
        led_controller_.UpdateLedStatus(power_manager_);

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(0);

        Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle);

        ESP_LOGI(TAG, "Device powered off.");
    }

    void InitializeMCP()
    {
        auto &mcp_server = McpServer::GetInstance();
        InitializeSystemMCP(mcp_server, power_manager_);
    }

public:
    AudioXiaoYa() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializePowerManager();
        InitializeLedController();
        InitializeAudioOutputControl();
        InitializeButtonCallbacks();
        PowerOn();
        InitializeMCP();

        power_manager_.SetPowerStateCallback([this](FogSeekPowerManager::PowerState state)
                                             { led_controller_.UpdateLedStatus(power_manager_); });
    }

    virtual Led *GetLed() override
    {
        return led_controller_.GetGreenLed();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual void StartNetwork() override
    {
        auto &wifi_manager = WifiManager::GetInstance();

        WifiManagerConfig config;
        config.ssid_prefix = "XiaoYa";
        config.language = Lang::CODE;
        wifi_manager.Initialize(config);

        wifi_manager.SetEventCallback([this, &wifi_manager](WifiEvent event)
                                      {
            std::string ssid = wifi_manager.GetSsid();
            switch (event) {
                case WifiEvent::Scanning:
                    OnNetworkEvent(NetworkEvent::Scanning);
                    break;
                case WifiEvent::Connecting:
                    OnNetworkEvent(NetworkEvent::Connecting, ssid);
                    break;
                case WifiEvent::Connected:
                    OnNetworkEvent(NetworkEvent::Connected, ssid);
                    break;
                case WifiEvent::Disconnected:
                    OnNetworkEvent(NetworkEvent::Disconnected);
                    break;
                case WifiEvent::ConfigModeEnter:
                    OnNetworkEvent(NetworkEvent::WifiConfigModeEnter);
                    break;
                case WifiEvent::ConfigModeExit:
                    OnNetworkEvent(NetworkEvent::WifiConfigModeExit);
                    break;
            } });

        TryWifiConnect();
    }

    ~AudioXiaoYa()
    {
    }
};

DECLARE_BOARD(AudioXiaoYa);