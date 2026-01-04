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
#include "device_state_machine.h"
#include <ssid_manager.h>
#include <wifi_manager.h>
#include <esp_log.h>
#include <driver/rtc_io.h>

#define TAG "FogSeekAudioLinkBit"

class FogSeekAudioLinkBit : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    FogSeekPowerManager power_manager_;
    FogSeekLedController led_controller_;
    AudioCodec *audio_codec_ = nullptr;
    esp_timer_handle_t check_idle_timer_ = nullptr;

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

    // 初始化音频输出控制
    void InitializeAudioOutputControl()
    {
        auto codec = GetAudioCodec();
        codec->SetOutputVolume(0); // 功放不支持使能控制，通过设置音量来替代使能，避免USB插入时自动播放声音
    }

    // 初始化按键回调
    void InitializeButtonCallbacks()
    {
        ctrl_button_.OnClick([this]()
                             {
                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 切换聊天状态（打断）
                             });
        ctrl_button_.OnDoubleClick([this]()
                                   {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting)
            {
                EnterWifiConfigMode();
                return;
            } });
        ctrl_button_.OnLongPress([this]()
                                 {
            // 切换电源状态
            if (!power_manager_.IsPowerOn()) {
                PowerOn();
            } else {
                PowerOff();
            } });
    }

    // 处理自动唤醒逻辑
    void HandleAutoWake()
    {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            auto &app = Application::GetInstance();
            // USB充电状态下开机需要播放音效
            if (power_manager_.IsUsbPowered())
            {
                app.PlaySound(Lang::Sounds::OGG_SUCCESS);
                vTaskDelay(pdMS_TO_TICKS(500)); // 延时500ms播放音效
            }
            app.Schedule([]()
                         {
                            auto &app = Application::GetInstance();
                            app.ToggleChatState(); });
        }
        else
        {
            // 设备尚未进入空闲状态，500ms后再次检查，使用定时器异步检查，不阻塞当前任务
            esp_timer_handle_t check_timer;
            esp_timer_create_args_t timer_args = {};
            timer_args.callback = [](void *arg)
            {
                auto instance = static_cast<FogSeekAudioLinkBit *>(arg);
                instance->HandleAutoWake();
            };
            timer_args.arg = this;
            timer_args.name = "check_idle_timer";
            esp_timer_create(&timer_args, &check_timer);
            esp_timer_start_once(check_timer, 500000); // 500ms = 500000微秒
        }
    }

    // 开机流程
    void PowerOn()
    {
        power_manager_.PowerOn();                        // 更新电源状态
        led_controller_.UpdateLedStatus(power_manager_); // 更新LED灯状态

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(70); // 开机后将音量设置为默认值

        ESP_LOGI(TAG, "Device powered on.");

        HandleAutoWake(); // 开机自动唤醒
    }

    // 关机流程
    void PowerOff()
    {
        power_manager_.PowerOff();
        led_controller_.UpdateLedStatus(power_manager_);

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(0); // 关机后将音量设置为默0

        Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle); // 关机后将设备状态设置为空闲，便于下次开机自动唤醒

        ESP_LOGI(TAG, "Device powered off.");
    }

public:
    FogSeekAudioLinkBit() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializePowerManager();
        InitializeLedController();
        InitializeAudioOutputControl();
        InitializeButtonCallbacks();

        // 设置电源状态变化回调函数，充电时，充电状态变化更新指示灯
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

    // 重写StartNetwork方法，实现自定义Wi-Fi热点名称
    virtual void StartNetwork() override
    {
        auto &wifi_manager = WifiManager::GetInstance();

        // Initialize WiFi manager with custom SSID prefix
        WifiManagerConfig config;
        config.ssid_prefix = "LinkBit";
        config.language = Lang::CODE;
        wifi_manager.Initialize(config);

        // Set unified event callback - forward to NetworkEvent with SSID data
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

        // Try to connect or enter config mode
        TryWifiConnect();
    }

    ~FogSeekAudioLinkBit()
    {
    }
};

DECLARE_BOARD(FogSeekAudioLinkBit);