#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>

#include "power_manager.h"
#include "esp_sleep.h"

#define TAG "FogSeekEsp32s3Audio"

class FogSeekEsp32s3Audio : public WifiBoard
{
private:
    Button boot_button_;
    Button pwr_button_;
    PowerManager *power_manager_;
    bool power_save_mode_ = false;

    // 状态记录变量
    bool is_charging_ = false;
    bool is_charge_done_ = false;
    uint8_t battery_level_ = 0;

    // 添加定时器句柄以便正确管理
    TimerHandle_t status_timer_ = nullptr;

    // LED初始化，LampController中会初始化LED，打开LampController这里需要注释
    void InitializeLeds()
    {
        gpio_config_t led_conf = {};
        led_conf.intr_type = GPIO_INTR_DISABLE;
        led_conf.mode = GPIO_MODE_OUTPUT;
        led_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO); // 红色灯通过MCP控制
        led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&led_conf);
        SetLedState(LED_RED_GPIO, false);
        SetLedState(LED_GREEN_GPIO, false);
    }

    void SetLedState(gpio_num_t gpio, bool state)
    {
        gpio_set_level(gpio, state ? 1 : 0);
    }

    void InitializePowerManager()
    {
        // 初始化电源控制引脚
        gpio_config_t pwr_conf = {};
        pwr_conf.intr_type = GPIO_INTR_DISABLE;
        pwr_conf.mode = GPIO_MODE_OUTPUT;
        pwr_conf.pin_bit_mask = (1ULL << PWR_CTRL_GPIO);
        pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&pwr_conf);
        // 初始化为关机状态
        gpio_set_level(PWR_CTRL_GPIO, 0);

        power_manager_ = new PowerManager(PWR_CHARGING_GPIO, PWR_CHARGE_DONE_GPIO);

        power_manager_->OnChargingStatusChanged([this](bool is_charging)
                                                {
            is_charging_ = is_charging;
            SetLedState(LED_RED_GPIO, true);
            SetLedState(LED_GREEN_GPIO, false); });

        power_manager_->OnChargeDoneStatusChanged([this](bool is_charge_done)
                                                  {
            is_charge_done_ = is_charge_done;
            SetLedState(LED_RED_GPIO, false);
            SetLedState(LED_GREEN_GPIO, true); });

        power_manager_->OnLowBatteryStatusChanged([this](bool is_low_battery)
                                                  {
            if (is_low_battery) {
                SetLedState(LED_RED_GPIO, true);
                SetLedState(LED_GREEN_GPIO, true);
                // 低电量状态保存下来，定时器统一打印
            } });
    }

    // 提取定时器回调函数为独立方法，便于管理和清理
    static void LogStatusCallback(TimerHandle_t xTimer)
    {
        FogSeekEsp32s3Audio *self = (FogSeekEsp32s3Audio *)pvTimerGetTimerID(xTimer);

        // 更新电池电量
        self->battery_level_ = self->power_manager_->GetBatteryLevel();

        ESP_LOGI(TAG, "System status - Red_LED: %s, Green_LED: %s, Charging: %s, Charge Done: %s, Battery: %d%%",
                 gpio_get_level(LED_RED_GPIO) ? "ON" : "OFF",
                 gpio_get_level(LED_GREEN_GPIO) ? "ON" : "OFF",
                 self->is_charging_ ? "Yes" : "No",
                 self->is_charge_done_ ? "Yes" : "No",
                 self->battery_level_);
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                // 重置 WiFi 配置
            }
            app.ToggleChatState(); });

        pwr_button_.OnLongPress([this]()
                                {
            ESP_LOGI(TAG, "Power button long press detected");

            // 检测USB是否插入（即判断is_charging_ || is_charge_done_）
            if (is_charging_ || is_charge_done_) {
                // 当有USB插入，按键无效，不做操作
                ESP_LOGI(TAG, "USB is connected, power button ignored");
                return;
            }

            // 当无USB插入
            if (!power_save_mode_) {
                // 打开电源
                gpio_set_level(PWR_CTRL_GPIO, 1);
                power_save_mode_ = true;
                ESP_LOGI(TAG, "Power control pin set to HIGH for keeping power.");

                // 开机亮绿灯
                SetLedState(LED_GREEN_GPIO, true);
            } else {
                // 当按键再次长按，则关闭电源
                gpio_set_level(PWR_CTRL_GPIO, 0);
                power_save_mode_ = false;
                ESP_LOGI(TAG, "Power control pin set to LOW for shutdown.");

                // 关闭双LED灯
                SetLedState(LED_RED_GPIO, false);
                SetLedState(LED_GREEN_GPIO, false);

                // 停止并删除定时器
                if (status_timer_ != nullptr) {
                    xTimerStop(status_timer_, 0);
                    xTimerDelete(status_timer_, 0);
                    status_timer_ = nullptr;
                }

                // 延时一段时间，确保电源完全关闭
                // vTaskDelay(pdMS_TO_TICKS(100));
                // esp_deep_sleep_start();  // 进入深度睡眠模式（真正关机）
            } });
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeIot()
    {
        // auto &thing_manager = iot::ThingManager::GetInstance();
        // thing_manager.AddThing(iot::CreateThing("Speaker"));
        // thing_manager.AddThing(iot::CreateThing("Lamp"));

        static LampController lamp(LED_RED_GPIO);
    }

public:
    FogSeekEsp32s3Audio() : boot_button_(BOOT_BUTTON_GPIO), pwr_button_(PWR_BUTTON_GPIO)
    {
        InitializeIot();
        InitializeLeds();
        InitializePowerManager();
        InitializeButtons();

        // 创建并启动定时器，每 5 秒打印一次系统状态
        if (status_timer_ == nullptr)
        {
            status_timer_ = xTimerCreate("LogStatus", pdMS_TO_TICKS(5000), pdTRUE, this, LogStatusCallback);
            if (status_timer_ != nullptr)
            {
                xTimerStart(status_timer_, 0);
                ESP_LOGI(TAG, "System status logging started.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to create status logging timer.");
            }
        }
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    // 添加析构函数以正确清理资源
    ~FogSeekEsp32s3Audio()
    {
        if (status_timer_ != nullptr)
        {
            xTimerStop(status_timer_, 0);
            xTimerDelete(status_timer_, 0);
        }
        if (power_manager_)
        {
            delete power_manager_;
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3Audio);