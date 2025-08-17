#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "led/gpio_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include <wifi_station.h>
#include <esp_log.h>

#define TAG "FogSeekMoodlight"

class FogSeekMoodlight : public WifiBoard
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

    // 冷暖色灯控制
    GpioLed *cold_light_ = nullptr;
    GpioLed *warm_light_ = nullptr;
    bool cold_light_state_ = false; // 添加冷灯状态变量
    bool warm_light_state_ = false; // 添加暖灯状态变量

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
        FogSeekMoodlight *self = static_cast<FogSeekMoodlight *>(arg);
        self->CheckLowBattery();
    }

    void InitializeLeds()
    {
        gpio_config_t led_conf = {};
        led_conf.intr_type = GPIO_INTR_DISABLE;
        led_conf.mode = GPIO_MODE_OUTPUT;
        led_conf.pin_bit_mask = (1ULL << LED_RED_GPIO) | (1ULL << LED_GREEN_GPIO);
        led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&led_conf);
        gpio_set_level(LED_RED_GPIO, 0);
        gpio_set_level(LED_GREEN_GPIO, 0);

        // 初始化冷暖色灯（使用PWM控制）
        // 为冷色灯和暖色灯分配不同的LEDC通道，避免冲突
        cold_light_ = new GpioLed(COLD_LIGHT_GPIO, 0, LEDC_TIMER_1, LEDC_CHANNEL_0);
        warm_light_ = new GpioLed(WARM_LIGHT_GPIO, 0, LEDC_TIMER_1, LEDC_CHANNEL_1);

        // 默认关闭所有灯
        cold_light_->TurnOff();
        warm_light_->TurnOff();
    }

    void InitializeMCP()
    {
        // 获取MCP服务器实例
        auto &mcp_server = McpServer::GetInstance();

        // 添加获取当前灯状态的工具函数
        mcp_server.AddTool("self.light.get_status", "获取当前灯的状态", PropertyList(), [this](const PropertyList &properties) -> ReturnValue
                           {
            // 使用字符串拼接方式返回JSON - 项目中最标准的做法
            std::string status = "{\"cold_light\":" + std::string(cold_light_state_ ? "true" : "false") + 
                                ",\"warm_light\":" + std::string(warm_light_state_ ? "true" : "false") + "}";
            return status; });

        // 添加设置冷暖灯光亮度的工具函数
        mcp_server.AddTool("self.light.set_brightness",
                           "设置冷暖灯光的亮度，冷光和暖光可以独立调节，亮度范围为0-100，关灯为0，开灯默认为30亮度。"
                           "根据用户情绪描述调节冷暖灯光亮度，大模型应该分析用户的话语，理解用户的情绪状态和场景描述，然后根据情绪设置合适的冷暖灯光亮度组合。",
                           PropertyList({Property("cold_brightness", kPropertyTypeInteger, 0, 100),
                                         Property("warm_brightness", kPropertyTypeInteger, 0, 100)}),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               // 使用operator[]而不是at()访问属性
                               int cold_brightness = properties["cold_brightness"].value<int>();
                               int warm_brightness = properties["warm_brightness"].value<int>();

                               cold_light_->SetBrightness(cold_brightness);
                               warm_light_->SetBrightness(warm_brightness);
                               cold_light_->TurnOn();
                               warm_light_->TurnOn();

                               // 更新状态
                               cold_light_state_ = cold_brightness > 0;
                               warm_light_state_ = warm_brightness > 0;

                               ESP_LOGI(TAG, "Color temperature set - Cold: %d%%, Warm: %d%%",
                                        cold_brightness, warm_brightness);

                               // 使用字符串拼接方式返回JSON - 项目中最标准的做法
                               std::string result = "{\"success\":true"
                                                    ",\"cold_brightness\":" +
                                                    std::to_string(cold_brightness) +
                                                    ",\"warm_brightness\":" + std::to_string(warm_brightness) + "}";
                               return result;
                           });
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
            .callback = &FogSeekMoodlight::BatteryCheckTimerCallback,
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
                                        gpio_set_level(LED_RED_GPIO, 0);
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
    FogSeekMoodlight() : boot_button_(BOOT_GPIO), pwr_button_(BUTTON_GPIO)
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

    ~FogSeekMoodlight()
    {
        if (battery_check_timer_)
        {
            esp_timer_stop(battery_check_timer_);
            esp_timer_delete(battery_check_timer_);
        }

        if (battery_monitor_)
        {
            delete battery_monitor_;
        }
    }
};

DECLARE_BOARD(FogSeekMoodlight);