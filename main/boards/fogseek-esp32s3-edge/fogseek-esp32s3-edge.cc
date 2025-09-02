#include "wifi_board.h"
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

#define TAG "FogSeekEsp32s3Edge"

class FogSeekEsp32s3Edge : public WifiBoard
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

    i2c_master_bus_handle_t i2c_bus_ = nullptr;

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
        FogSeekEsp32s3Edge *self = static_cast<FogSeekEsp32s3Edge *>(arg);
        self->CheckLowBattery();
    }

    // 说话状态LED闪烁定时器回调
    static void SpeakingBlinkTimerCallback(void *arg)
    {
        FogSeekEsp32s3Edge *self = static_cast<FogSeekEsp32s3Edge *>(arg);
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
            .callback = &FogSeekEsp32s3Edge::SpeakingBlinkTimerCallback,
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
        battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_2, ADC_CHANNEL_4, 2.0f, 1.0f, PWR_CHARGE_DONE_GPIO);
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
            .callback = &FogSeekEsp32s3Edge::BatteryCheckTimerCallback,
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

    void InitializeI2c()
    {
        // Initialize I2C peripheral
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

public:
    FogSeekEsp32s3Edge() : boot_button_(BOOT_GPIO), pwr_button_(BUTTON_GPIO)
    {
        InitializeI2c();
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
        // 使用ES8311编解码器实现全双工对讲和语音唤醒打断
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
            true, // use_mclk
            false // pa_inverted
        );
        return &audio_codec;
    }

    ~FogSeekEsp32s3Edge()
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

        if (i2c_bus_)
        {
            i2c_del_master_bus(i2c_bus_);
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3Edge);