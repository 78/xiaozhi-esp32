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
#include <driver/spi_master.h>
#include "display/lcd_display.h"
#include "backlight.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st77916.h>
#include <driver/ledc.h>

#define TAG "FogSeekEsp32s3EdgeLcd15"

// 声明可用的字体
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

static const st77916_lcd_init_cmd_t lcd_init_cmds[] = {
    // Initial setup
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},
    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},

    // Power settings
    {0xB0, (uint8_t[]){0x69}, 1, 0},
    {0xB1, (uint8_t[]){0x4A}, 1, 0},
    {0xB2, (uint8_t[]){0x2F}, 1, 0},
    {0xB3, (uint8_t[]){0x01}, 1, 0},
    {0xB4, (uint8_t[]){0x69}, 1, 0},
    {0xB5, (uint8_t[]){0x45}, 1, 0},
    {0xB6, (uint8_t[]){0xAB}, 1, 0},
    {0xB7, (uint8_t[]){0x41}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xB9, (uint8_t[]){0x15}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x07}, 1, 0},

    // More power settings
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},
    {0xCC, (uint8_t[]){0x7F}, 1, 0},
    {0xCD, (uint8_t[]){0x7F}, 1, 0},
    {0xCE, (uint8_t[]){0xFF}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},

    // Gamma settings
    {0xE0, (uint8_t[]){0xF0, 0x10, 0x18, 0x0D, 0x0C, 0x38, 0x3E, 0x44, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x0F, 0x17, 0x0D, 0x0B, 0x07, 0x3E, 0x33, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34}, 14, 0},

    // More settings
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x08}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x03}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xE9}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x14}, 1, 0},
    {0xEE, (uint8_t[]){0xFF}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0xFF}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x30}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},

    // Display settings
    {0x60, (uint8_t[]){0x40}, 1, 0},
    {0x61, (uint8_t[]){0x05}, 1, 0},
    {0x62, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x42}, 1, 0},
    {0x64, (uint8_t[]){0xDA}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x40}, 1, 0},
    {0x71, (uint8_t[]){0x04}, 1, 0},
    {0x72, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x42}, 1, 0},
    {0x74, (uint8_t[]){0xD9}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},

    // More display settings
    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x07}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xD7}, 1, 0},
    {0x85, (uint8_t[]){0x04}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x09}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xD9}, 1, 0},
    {0x8D, (uint8_t[]){0x04}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},
    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x0B}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xDB}, 1, 0},
    {0x95, (uint8_t[]){0x04}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0D}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xDD}, 1, 0},
    {0x9D, (uint8_t[]){0x04}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},
    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x06}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xD6}, 1, 0},
    {0xA5, (uint8_t[]){0x04}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x08}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xD8}, 1, 0},
    {0xAD, (uint8_t[]){0x04}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x0A}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xDA}, 1, 0},
    {0xB5, (uint8_t[]){0x04}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0C}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xDC}, 1, 0},
    {0xBD, (uint8_t[]){0x04}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x10}, 1, 0},
    {0xC1, (uint8_t[]){0x47}, 1, 0},
    {0xC2, (uint8_t[]){0x56}, 1, 0},
    {0xC3, (uint8_t[]){0x65}, 1, 0},
    {0xC4, (uint8_t[]){0x74}, 1, 0},
    {0xC5, (uint8_t[]){0x88}, 1, 0},
    {0xC6, (uint8_t[]){0x99}, 1, 0},
    {0xC7, (uint8_t[]){0x01}, 1, 0},
    {0xC8, (uint8_t[]){0xBB}, 1, 0},
    {0xC9, (uint8_t[]){0xAA}, 1, 0},
    {0xD0, (uint8_t[]){0x10}, 1, 0},
    {0xD1, (uint8_t[]){0x47}, 1, 0},
    {0xD2, (uint8_t[]){0x56}, 1, 0},
    {0xD3, (uint8_t[]){0x65}, 1, 0},
    {0xD4, (uint8_t[]){0x74}, 1, 0},
    {0xD5, (uint8_t[]){0x88}, 1, 0},
    {0xD6, (uint8_t[]){0x99}, 1, 0},
    {0xD7, (uint8_t[]){0x01}, 1, 0},
    {0xD8, (uint8_t[]){0xBB}, 1, 0},
    {0xD9, (uint8_t[]){0xAA}, 1, 0},
    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},

    // Final display setup
    {0x3A, (uint8_t[]){0x05}, 1, 0},   // Pixel format
    {0x35, (uint8_t[]){0x00}, 1, 0},   // Tearing effect line
    {0x21, (uint8_t[]){0x00}, 1, 0},   // Display inversion
    {0x11, (uint8_t[]){0x00}, 1, 120}, // Sleep out with 120ms delay
    {0x29, (uint8_t[]){0x00}, 1, 0},   // Display on
};

class FogSeekEsp32s3EdgeLcd15 : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    AdcBatteryMonitor *battery_monitor_;
    PwmBacklight *pwm_test_ = nullptr; // 添加这个成员变量来保存pwm_test指针
    bool no_dc_power_ = false;
    bool pwr_hold_state_ = false;
    bool low_battery_warning_ = false;
    bool low_battery_shutdown_ = false;
    esp_timer_handle_t battery_check_timer_ = nullptr;
    esp_timer_handle_t speaking_blink_timer_ = nullptr;
    bool speaking_led_state_ = false;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    AudioCodec *audio_codec_ = nullptr;
    Display *display_ = nullptr;
    Backlight *backlight_ = nullptr;

    void UpdateBatteryStatus()
    {
        bool is_charging_pin = gpio_get_level(PWR_CHARGING_GPIO) == 0;   // CHRG引脚，低电平表示正在充电
        bool is_charge_done = gpio_get_level(PWR_CHARGE_DONE_GPIO) == 0; // STDBY引脚，低电平表示充电完成
        uint8_t battery_level = 80;                                      // 使用固定值代替ADC读取

        bool battery_detected = battery_level > 0; // 通过固定值判断电池是否存在

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
            gpio_set_level(LED_RED_GPIO, 0);
            gpio_set_level(LED_GREEN_GPIO, 1);
            ESP_LOGI(TAG, "Battery is charging, level: %d%%", battery_level);
        }
        else if (is_charge_done)
        {
            // 充电完成状态（CHRG高电平，STDBY低电平）
            no_dc_power_ = false;
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 0);
            ESP_LOGI(TAG, "Battery charge completed, level: %d%%", battery_level);
        }
        else
        {
            // 无电池状态
            no_dc_power_ = false;
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 1);
            ESP_LOGI(TAG, "No battery detected");
        }

        // 更新显示屏状态
        if (display_)
        {
            char status[64];
            snprintf(status, sizeof(status), "电池: %d%%", battery_level);
            display_->SetStatus(status);
        }
    }

    // 低电量检测逻辑
    void CheckLowBattery()
    {
        uint8_t battery_level = 80; // 使用固定值代替ADC读取

        if (no_dc_power_)
        {
            // 低于10%自动关机，使用CONFIG_OCV_SOC_MODEL_2，最低电压是 3.305545V（对应0%电量）
            if (battery_level < 10 && !low_battery_shutdown_)
            {
                ESP_LOGW(TAG, "Critical battery level (%d%%), shutting down to protect battery", battery_level);
                low_battery_shutdown_ = true;

                auto &app = Application::GetInstance();
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY); // 关机

                pwr_hold_state_ = false;
                gpio_set_level(PWR_HOLD_GPIO, 0); // 关闭电源
                gpio_set_level(LED_RED_GPIO, 1);
                gpio_set_level(LED_GREEN_GPIO, 1);
                ESP_LOGI(TAG, "Device shut down due to critical battery level");
                return;
            }
            // 低于20%警告
            else if (battery_level < 20 && battery_level >= 10 && !low_battery_warning_)
            {
                gpio_set_level(LED_RED_GPIO, 0);
                gpio_set_level(LED_GREEN_GPIO, 1);
                ESP_LOGW(TAG, "Low battery warning (%d%%)", battery_level);
                low_battery_warning_ = true;

                auto &app = Application::GetInstance();
                app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY); // 发送低电量警告通知

                // 在显示屏上显示低电量警告
                if (display_)
                {
                    display_->SetStatus("低电量警告");
                }
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
        FogSeekEsp32s3EdgeLcd15 *self = static_cast<FogSeekEsp32s3EdgeLcd15 *>(arg);
        self->CheckLowBattery();
    }

    // 说话状态LED闪烁定时器回调
    static void SpeakingBlinkTimerCallback(void *arg)
    {
        FogSeekEsp32s3EdgeLcd15 *self = static_cast<FogSeekEsp32s3EdgeLcd15 *>(arg);
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
            if (display_)
            {
                display_->SetStatus("空闲");
                display_->SetChatMessage("system", "等待唤醒...");
            }
            break;

        case kDeviceStateListening:
            // 唤醒状态，两个LED同时亮起
            gpio_set_level(LED_RED_GPIO, 0);
            gpio_set_level(LED_GREEN_GPIO, 0);
            if (display_)
            {
                display_->SetStatus("监听中");
                display_->SetChatMessage("system", "正在聆听...");
            }
            break;

        case kDeviceStateSpeaking:
            // 说话状态，两个LED同时闪烁
            speaking_led_state_ = false;
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 1);
            // 启动闪烁定时器，每500ms切换一次状态
            esp_timer_start_periodic(speaking_blink_timer_, 500 * 1000);
            if (display_)
            {
                display_->SetStatus("回答中");
                display_->SetChatMessage("system", "正在回答...");
            }
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
        led_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO);
        led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&led_conf);
        gpio_set_level(LED_GREEN_GPIO, 1);

        // 创建说话状态LED闪烁定时器
        esp_timer_create_args_t timer_args = {
            .callback = &FogSeekEsp32s3EdgeLcd15::SpeakingBlinkTimerCallback,
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
        // 由于硬件设计问题，ADC引脚被占用，无法正常使用电池监控功能
        // battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_1, 2.0f, 1.0f, PWR_CHARGE_DONE_GPIO);
        // 使用固定值(80%)来模拟电池状态
        battery_monitor_ = nullptr; // 不使用实际的电池监控器

        // 初始化充电检测引脚（CHRG引脚）
        gpio_config_t charge_conf = {};
        charge_conf.intr_type = GPIO_INTR_DISABLE;
        charge_conf.mode = GPIO_MODE_INPUT;
        charge_conf.pin_bit_mask = (1ULL << PWR_CHARGING_GPIO);
        charge_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        charge_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&charge_conf);

        // 初始化充电完成检测引脚（STDBY引脚）
        gpio_config_t charge_done_conf = {};
        charge_done_conf.intr_type = GPIO_INTR_DISABLE;
        charge_done_conf.mode = GPIO_MODE_INPUT;
        charge_done_conf.pin_bit_mask = (1ULL << PWR_CHARGE_DONE_GPIO);
        charge_done_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        charge_done_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&charge_done_conf);

        // 注册充电状态变化回调
        // battery_monitor_->OnChargingStatusChanged([this](bool is_charging)
        //                                           { UpdateBatteryStatus(); });
        UpdateBatteryStatus(); // 初始化时立即更新LED状态

        // 低电量管理 - 创建电池检查定时器，每30秒检查一次
        esp_timer_create_args_t timer_args = {
            .callback = &FogSeekEsp32s3EdgeLcd15::BatteryCheckTimerCallback,
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
        pwr_conf.pin_bit_mask = (1ULL << PWR_HOLD_GPIO);
        pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&pwr_conf);
        gpio_set_level(PWR_HOLD_GPIO, 0); // 初始化为关机状态

        // 短按打断
        ctrl_button_.OnClick([this]()
                             {
                                 ESP_LOGI(TAG, "Button clicked");
                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 聊天状态切换（包括打断）
                             });

        // 长按开关机
        ctrl_button_.OnLongPress([this]()
                                 {
                                    // 切换电源状态
                                    if(!pwr_hold_state_) {
                                        pwr_hold_state_ = true;
                                        gpio_set_level(PWR_HOLD_GPIO, 1);   // 打开电源
                                        gpio_set_level(LED_GREEN_GPIO, 0);  // 打开指示灯
                                        backlight_->RestoreBrightness();    // 打开屏幕
                                        // audio_codec_->EnableOutput(true);   // 打开功放
                                        gpio_set_level(AUDIO_CODEC_PA_PIN, 1);
                                        ESP_LOGI(TAG, "Power control pin set to HIGH for keeping power.");
                                    } 
                                    else{
                                        pwr_hold_state_ = false;
                                        backlight_->SetBrightness(0);          // 关闭屏幕
                                        // audio_codec_->EnableOutput(false);     // 关闭功放
                                        gpio_set_level(AUDIO_CODEC_PA_PIN, 0);
                                        if (!no_dc_power_)
                                        {
                                            UpdateBatteryStatus(); // 恢复LED灯充电状态
                                        }
                                        else
                                        {
                                            gpio_set_level(LED_RED_GPIO, 1); // 关闭指示灯
                                            gpio_set_level(LED_GREEN_GPIO, 1); // 关闭指示灯
                                        }
                                        gpio_set_level(PWR_HOLD_GPIO, 0); // 当按键再次长按，则关闭电源
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

    // 初始化LCD显示屏
    void InitializeDisplay()
    {
        ESP_LOGI(TAG, "Initializing LCD display");

        // Initialize SPI bus using standard ESP-IDF configuration
        const spi_bus_config_t bus_config = {
            .data0_io_num = LCD_IO0_GPIO,
            .data1_io_num = LCD_IO1_GPIO,
            .sclk_io_num = LCD_SCL_GPIO,
            .data2_io_num = LCD_IO2_GPIO,
            .data3_io_num = LCD_IO3_GPIO,
            .max_transfer_sz = 4096,
            .flags = SPICOMMON_BUSFLAG_QUAD,
            .intr_flags = 0};

        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

        // QSPI接口IO配置，使用ESP-IDF标准宏
        const esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(
            LCD_CS_GPIO,
            NULL,
            NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &panel_io_));

        // ST77916面板配置
        st77916_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77916_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            },
        };

        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_RESET_GPIO,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = LCD_BIT_PER_PIXEL,
            .flags = {
                .reset_active_high = 0,
            },
            .vendor_config = &vendor_config,
        };

        // 创建ST77916面板
        ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(panel_io_, &panel_config, &panel_));

        // 重置和初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));

        // 打开显示
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        // 初始化背光（使用静态对象而不是动态分配）
        backlight_ = new PwmBacklight(LCD_BL_GPIO, false);
        backlight_->SetBrightness(0); // 初始化关闭屏幕背光

        // 创建SPI LCD显示对象
        display_ = new SpiLcdDisplay(
            panel_io_,
            panel_,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X,
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY,
            {
                .text_font = &font_puhui_20_4,
                .icon_font = &font_awesome_20_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                .emoji_font = font_emoji_32_init(),
#else
                .emoji_font = font_emoji_64_init(),
#endif
            });

        // 延迟一段时间让显示和LVGL完全初始化
        vTaskDelay(pdMS_TO_TICKS(200));
    }

public:
    FogSeekEsp32s3EdgeLcd15() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeLeds();
        InitializeMCP();
        InitializeDisplay();
        InitializeBatteryMonitor();
        InitializeButtons();
        // 使用PwmBacklight类来生成PWM信号进行测试
        // 使用100Hz频率，便于示波器观察且避免错误
        PwmBacklight *pwm_test = new PwmBacklight(GPIO_NUM_42, false, 100);
        // 保存pwm_test指针以避免未使用变量警告
        pwm_test_ = pwm_test;
        // 设置50%占空比
        pwm_test->SetBrightness(50);

        // 初始化音频编解码器，默认关闭
        // audio_codec_ = GetAudioCodec();
        // audio_codec_->EnableOutput(false);
        gpio_set_level(AUDIO_CODEC_PA_PIN, 0);
        // 注册设备状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(
            [this](DeviceState previous_state, DeviceState current_state)
            {
                OnDeviceStateChanged(previous_state, current_state);
            });
    }

    virtual Display *GetDisplay() override
    {
        return display_;
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
            true,
            false);
        return &audio_codec;
    }

    ~FogSeekEsp32s3EdgeLcd15()
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

        if (i2c_bus_)
        {
            i2c_del_master_bus(i2c_bus_);
        }

        if (display_)
        {
            delete display_;
        }

        if (panel_)
        {
            esp_lcd_panel_del(panel_);
        }

        if (panel_io_)
        {
            esp_lcd_panel_io_del(panel_io_);
        }
    }
};

DECLARE_BOARD(FogSeekEsp32s3EdgeLcd15);