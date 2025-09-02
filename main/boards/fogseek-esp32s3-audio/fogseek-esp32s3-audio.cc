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
#include "adc_battery_monitor.h"
#include <wifi_station.h>
#include <esp_log.h>

#define TAG "FogSeekEsp32s3Audio"

class FogSeekEsp32s3Audio : public WifiBoard
{
private:
    Button boot_button_;
    Button pwr_button_;
    AdcBatteryMonitor *battery_monitor_;
    bool no_dc_power_ = false;
    bool pwr_ctrl_state_ = false;
    bool low_battery_warning_ = false;
    bool low_battery_shutdown_ = false;
    esp_timer_handle_t battery_check_timer_ = nullptr;
    esp_timer_handle_t speaking_blink_timer_ = nullptr;
    bool speaking_led_state_ = false;
    bool first_idle_state_ = true;

    void UpdateBatteryStatus()
    {
        bool is_charging_pin = gpio_get_level(PWR_CHARGING_GPIO) == 0;   // CHRG引脚，低电平表示正在充电
        bool is_charge_done = gpio_get_level(PWR_CHARGE_DONE_GPIO) == 0; // STDBY引脚，低电平表示充电完成
        uint8_t battery_level = battery_monitor_->GetBatteryLevel();
        bool battery_detected = battery_level > 0; // 通过ADC检测电池是否存在

        if (battery_detected && !is_charging_pin && !is_charge_done)
        {
            // 有电池但未充电状态（CHRG高电平，STDBY高电平，无充电器）
            no_dc_power_ = true;
            ESP_LOGI(TAG, "Battery present but not charging, level: %d%%", battery_level);
        }
        else if (is_charging_pin)
        {
            // 正在充电状态（CHRG低电平，STDBY高电平）
            no_dc_power_ = false;
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 0);
            ESP_LOGI(TAG, "Battery is charging, level: %d%%", battery_level);
        }
        else if (is_charge_done)
        {
            // 充电完成状态（CHRG高电平，STDBY低电平）
            no_dc_power_ = false;
            gpio_set_level(LED_RED_GPIO, 0);
            gpio_set_level(LED_GREEN_GPIO, 1);
            ESP_LOGI(TAG, "Battery charge completed, level: %d%%", battery_level);
        }
        else
        {
            // 无电池状态
            no_dc_power_ = false;
            gpio_set_level(LED_RED_GPIO, 0);
            gpio_set_level(LED_GREEN_GPIO, 0);
            ESP_LOGI(TAG, "No battery detected");
        }
    }

    // 低电量检测逻辑
    void CheckLowBattery()
    {
        uint8_t battery_level = battery_monitor_->GetBatteryLevel();

        if (no_dc_power_)
        {
            // 低于10%自动关机，使用CONFIG_OCV_SOC_MODEL_2，最低电压是 3.305545V（对应0%电量）
            if (battery_level < 10 && !low_battery_shutdown_)
            {
                ESP_LOGW(TAG, "Critical battery level (%d%%), shutting down to protect battery", battery_level);
                low_battery_shutdown_ = true;

                auto &app = Application::GetInstance();
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY); // 关机
                vTaskDelay(pdMS_TO_TICKS(500));
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
                vTaskDelay(pdMS_TO_TICKS(500));
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
                vTaskDelay(pdMS_TO_TICKS(500));

                pwr_ctrl_state_ = false;
                gpio_set_level(PWR_CTRL_GPIO, 0); // 关闭电源
                gpio_set_level(LED_RED_GPIO, 0);
                gpio_set_level(LED_GREEN_GPIO, 0);
                ESP_LOGI(TAG, "Device shut down due to critical battery level");
                return;
            }
            // 低于20%警告
            else if (battery_level < 20 && battery_level >= 10 && !low_battery_warning_)
            {
                gpio_set_level(LED_RED_GPIO, 1);
                gpio_set_level(LED_GREEN_GPIO, 0);
                ESP_LOGW(TAG, "Low battery warning (%d%%)", battery_level);
                low_battery_warning_ = true;

                auto &app = Application::GetInstance();
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY); // 发送低电量警告通知
                vTaskDelay(pdMS_TO_TICKS(500));
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
                vTaskDelay(pdMS_TO_TICKS(500));
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            // 电量恢复到20%以上时重置警告标志
            else if (battery_level >= 20)
            {
                low_battery_warning_ = false;
            }
        }
        else
        {
            // 正在充电或充电完成时重置标志
            low_battery_warning_ = false;
            low_battery_shutdown_ = false;
        }
    }

    static void BatteryCheckTimerCallback(void *arg)
    {
        FogSeekEsp32s3Audio *self = static_cast<FogSeekEsp32s3Audio *>(arg);
        self->CheckLowBattery();
    }

    // 说话状态LED闪烁定时器回调
    static void SpeakingBlinkTimerCallback(void *arg)
    {
        FogSeekEsp32s3Audio *self = static_cast<FogSeekEsp32s3Audio *>(arg);
        self->speaking_led_state_ = !self->speaking_led_state_;
        gpio_set_level(LED_RED_GPIO, self->speaking_led_state_);
        gpio_set_level(LED_GREEN_GPIO, self->speaking_led_state_);
    }

    // 设备状态改变处理函数
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state)
    {
        // 停止说话状态闪烁定时器
        if (speaking_blink_timer_)
        {
            esp_timer_stop(speaking_blink_timer_);
        }

        switch (current_state)
        {
        case kDeviceStateIdle:
            // 休眠状态，显示充电状态颜色
            UpdateBatteryStatus();

            // 设备完成启动后自动进入对话状态
            // 打印状态转换信息以便调试
            ESP_LOGI(TAG, "Device state changed from %d to %d", previous_state, current_state);

            // 检查是否是首次进入空闲状态（设备启动完成）
            if (first_idle_state_)
            {
                first_idle_state_ = false;
                ESP_LOGI(TAG, "Device started, scheduling auto wake check");

                // 使用一个任务来检查应用程序是否完全准备好
                xTaskCreate([](void *arg)
                            {
                                // 等待应用程序完全初始化完成
                                auto &app = Application::GetInstance();

                                // 等待足够长的时间确保所有子系统都已准备好
                                // 音频系统、网络连接等需要一些时间来完全初始化
                                ESP_LOGI(TAG, "Waiting for full system initialization");
                                vTaskDelay(pdMS_TO_TICKS(5000)); // 等待5秒确保系统完全就绪

                                // 检查当前状态是否仍然稳定
                                if (app.GetDeviceState() == kDeviceStateIdle)
                                {
                                    ESP_LOGI(TAG, "System fully initialized, triggering auto wake");
                                    // 触发自动唤醒
                                    app.WakeWordInvoke("Hi,小雾");
                                }
                                else
                                {
                                    ESP_LOGW(TAG, "System state changed during initialization, skipping auto wake");
                                }

                                vTaskDelete(NULL); // 删除任务自身
                            },
                            "auto_wake_task", 4096, NULL, 5, NULL);
            }
            else
            {
                ESP_LOGI(TAG, "Not first idle state, skipping auto wake");
            }
            break;

        case kDeviceStateListening:
            // 唤醒状态，两个LED同时亮起（红灯亮，绿灯灭）
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 1);
            break;

        case kDeviceStateSpeaking:
            // 说话状态，两个LED同时闪烁
            speaking_led_state_ = false;
            gpio_set_level(LED_RED_GPIO, 0);
            gpio_set_level(LED_GREEN_GPIO, 0);
            // 启动闪烁定时器，每500ms切换一次状态
            esp_timer_start_periodic(speaking_blink_timer_, 500 * 1000);
            break;

        default:
            break;
        }
    }

    void InitializeLeds()
    {
        gpio_config_t led_conf = {};
        led_conf.intr_type = GPIO_INTR_DISABLE;
        led_conf.mode = GPIO_MODE_OUTPUT;
        led_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO) | (1ULL << LED_RED_GPIO);
        led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&led_conf);
        gpio_set_level(LED_GREEN_GPIO, 0);
        gpio_set_level(LED_RED_GPIO, 0);

        // 创建说话状态LED闪烁定时器
        esp_timer_create_args_t timer_args = {
            .callback = &FogSeekEsp32s3Audio::SpeakingBlinkTimerCallback,
            .arg = this,
            .name = "speaking_blink_timer"};
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &speaking_blink_timer_));
    }

    void InitializeMCP()
    {
        static LampController lamp(LED_RED_GPIO); // 红灯通过MCP控制
    }

    void InitializeBatteryMonitor()
    {
        // 使用通用的电池监测器处理电池电量检测
        battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_2, 2.0f, 1.0f, PWR_CHARGE_DONE_GPIO);
        // 初始化充电检测引脚（CHRG引脚）
        gpio_config_t charge_conf = {};
        charge_conf.intr_type = GPIO_INTR_DISABLE;
        charge_conf.mode = GPIO_MODE_INPUT;
        charge_conf.pin_bit_mask = (1ULL << PWR_CHARGING_GPIO);
        charge_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        charge_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&charge_conf);

        // 注册充电状态变化回调
        battery_monitor_->OnChargingStatusChanged([this](bool is_charging)
                                                  { UpdateBatteryStatus(); });

        UpdateBatteryStatus(); // 初始化时立即更新LED状态

        // 低电量管理 - 创建电池检查定时器，每30秒检查一次
        esp_timer_create_args_t timer_args = {
            .callback = &FogSeekEsp32s3Audio::BatteryCheckTimerCallback,
            .arg = this,
            .name = "battery_check_timer"};
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_check_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(battery_check_timer_, 30 * 1000 * 1000)); // 每30秒检查一次
    }

    void InitializeButtons()
    {
        // 初始化电源控制引脚
        gpio_config_t pwr_conf = {};
        pwr_conf.intr_type = GPIO_INTR_DISABLE;
        pwr_conf.mode = GPIO_MODE_OUTPUT;
        pwr_conf.pin_bit_mask = (1ULL << PWR_CTRL_GPIO);
        pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&pwr_conf);
        gpio_set_level(PWR_CTRL_GPIO, 0); // 初始化为关机状态

        // 短按打断
        pwr_button_.OnClick([this]()
                            {
                                ESP_LOGI(TAG, "Button clicked");
                                auto &app = Application::GetInstance();
                                app.ToggleChatState(); // 聊天状态切换（包括打断）
                            });

        // 长按开关机
        pwr_button_.OnLongPress([this]()
                                {
                                    if(!no_dc_power_) {
                                        ESP_LOGI(TAG, "DC power connected, power button ignored");
                                        return;
                                    }
                                    // 切换电源状态
                                    if(!pwr_ctrl_state_) {
                                        pwr_ctrl_state_ = true;
                                        gpio_set_level(PWR_CTRL_GPIO, 1);   // 打开电源
                                        gpio_set_level(LED_GREEN_GPIO, 1);
                                        ESP_LOGI(TAG, "Power control pin set to HIGH for keeping power.");
                                    } 
                                    else{
                                        pwr_ctrl_state_ = false;
                                        gpio_set_level(LED_RED_GPIO, 0);
                                        gpio_set_level(LED_GREEN_GPIO, 0);
                                        gpio_set_level(PWR_CTRL_GPIO, 0);   // 当按键再次长按，则关闭电源
                                        ESP_LOGI(TAG, "Power control pin set to LOW for shutdown.");
                                    } });
    }

public:
    FogSeekEsp32s3Audio() : boot_button_(BOOT_GPIO), pwr_button_(BUTTON_GPIO)
    {
        InitializeLeds();
        InitializeMCP();
        InitializeBatteryMonitor();
        InitializeButtons();

        // 注册设备状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(
            [this](DeviceState previous_state, DeviceState current_state)
            {
                OnDeviceStateChanged(previous_state, current_state);
            });
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    ~FogSeekEsp32s3Audio()
    {
        if (battery_check_timer_)
        {
            esp_timer_stop(battery_check_timer_);
            esp_timer_delete(battery_check_timer_);
        }

        if (speaking_blink_timer_)
        {
            esp_timer_stop(speaking_blink_timer_);
            esp_timer_delete(speaking_blink_timer_);
        }

        if (battery_monitor_)
        {
            delete battery_monitor_;
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3Audio);