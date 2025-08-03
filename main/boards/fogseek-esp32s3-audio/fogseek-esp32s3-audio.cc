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

    void InitializeLeds()
    {
        gpio_config_t led_conf = {};
        led_conf.intr_type = GPIO_INTR_DISABLE;
        led_conf.mode = GPIO_MODE_OUTPUT;
        led_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO); // 绿灯初始化
        led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&led_conf);
        gpio_set_level(LED_GREEN_GPIO, 0);
    }

    void InitializeMCP()
    {
        static LampController lamp(LED_RED_GPIO); // 红灯通过MCP控制
    }

    void InitializeBatteryMonitor()
    {
        // 使用通用的电池监测器处理电池电量检测
        battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_2, 4.2f, 3.6f, PWR_CHARGE_DONE_GPIO);
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

        // 低电量管理
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
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            app.ToggleChatState(); }); // 聊天状态切换（包括打断）

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
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    ~FogSeekEsp32s3Audio()
    {
        if (battery_monitor_)
        {
            delete battery_monitor_;
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3Audio);