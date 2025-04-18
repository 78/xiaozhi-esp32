#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_save_timer.h"
#include "../zhengchen-1.54tft-wifi/power_manager.h"

#include <wifi_station.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "ZHENGCHEN_0_96OLED_WIFI"

// 声明字体
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);


// 定义XINGZHI_CUBE_0_96OLED_WIFI类，继承自WifiBoard
class XINGZHI_CUBE_0_96OLED_WIFI : public WifiBoard {
private:
    // 定义I2C总线句柄
    i2c_master_bus_handle_t display_i2c_bus_;
    // 定义按钮
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    // 定义显示
    Display* display_;
    // 定义电源管理器
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    // 定义LCD面板IO句柄和面板句柄
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    // 初始化电源管理器
    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_9);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    // 初始化电源节能定时器
    // 初始化省电计时器
    void InitializePowerSaveTimer() {
        // 初始化GPIO 21
        rtc_gpio_init(GPIO_NUM_21);
        // 设置GPIO 21为输出模式
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);
        // 设置GPIO 21的电平为高
        rtc_gpio_set_level(GPIO_NUM_21, 1);

        // 创建省电计时器对象
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        // 设置进入睡眠模式时的回调函数
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            // 获取显示对象
            auto display = GetDisplay();
            // 设置聊天消息
            display->SetChatMessage("system", "");
            // 设置表情为困倦
            display->SetEmotion("sleepy");
        });
        // 设置退出睡眠模式时的回调函数
        power_save_timer_->OnExitSleepMode([this]() {
            // 获取显示对象
            auto display = GetDisplay();
            // 设置聊天消息
            display->SetChatMessage("system", "");
            // 设置表情为中性
            display->SetEmotion("neutral");
        });
        /* // 设置关机请求时的回调函数
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            // 设置GPIO 21的电平为低
            rtc_gpio_set_level(GPIO_NUM_21, 0);
            // 启用保持功能，确保睡眠期间电平不变
            rtc_gpio_hold_en(GPIO_NUM_21);
            esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
            esp_deep_sleep_start();
        }); */
        power_save_timer_->SetEnabled(true);
    }

    // 初始化I2C显示总线
    void InitializeDisplayI2c() {
        // 定义I2C总线配置
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0, // I2C端口
            .sda_io_num = DISPLAY_SDA_PIN, // SDA引脚
            .scl_io_num = DISPLAY_SCL_PIN, // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 误差忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };
        // 创建新的I2C主总线
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C, // I2C设备地址
            .on_color_trans_done = nullptr, // 颜色传输完成回调函数
            .user_ctx = nullptr, // 用户上下文
            .control_phase_bytes = 1, // 控制阶段字节数
            .dc_bit_offset = 6, // 数据/命令位偏移
            .lcd_cmd_bits = 8, // LCD命令位
            .lcd_param_bits = 8, // LCD参数位
            .flags = {
                .dc_low_on_data = 0, // 数据位为低电平
                .disable_control_phase = 0, // 禁用控制阶段
            },
            .scl_speed_hz = 400 * 1000, // SCL速度
        };

        // 创建新的I2C接口
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1; // 复位GPIO引脚
        panel_config.bits_per_pixel = 1; // 每个像素的位数

        // SSD1306配置
        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT), // 显示高度
        };
        panel_config.vendor_config = &ssd1306_config;

        // 创建新的SSD1306驱动
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    // 初始化按钮
    void InitializeButtons() {
        // 设置开机按钮的点击事件
        boot_button_.OnClick([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 获取应用程序实例
            auto& app = Application::GetInstance();
            // 如果设备状态为启动中且WiFi未连接，则重置WiFi配置
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            // 切换聊天状态
            app.ToggleChatState();
        });

        // 设置开机按钮的长按事件（直接进入配网模式）
        boot_button_.OnLongPress([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 获取应用程序实例
            auto& app = Application::GetInstance();
            
            // 进入配网模式
            app.SetDeviceState(kDeviceStateWifiConfiguring);
            
            // 重置WiFi配置以确保进入配网模式
            ResetWifiConfiguration();
        });

        // 设置音量增加按钮的点击事件
        volume_up_button_.OnClick([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 获取音频编解码器
            auto codec = GetAudioCodec();
            // 音量增加10
            auto volume = codec->output_volume() + 10;
            // 如果音量超过100，则设置为100
            if (volume > 100) {
                volume = 100;
            }
            // 设置输出音量
            codec->SetOutputVolume(volume);
            // 显示通知
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume/10));
        });

        // 设置音量增加按钮的长按事件
        volume_up_button_.OnLongPress([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 设置输出音量为最大
            GetAudioCodec()->SetOutputVolume(100);
            // 显示通知
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        // 设置音量减少按钮的点击事件
        volume_down_button_.OnClick([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 获取音频编解码器
            auto codec = GetAudioCodec();
            // 音量减少10
            auto volume = codec->output_volume() - 10;
            // 如果音量小于0，则设置为0
            if (volume < 0) {
                volume = 0;
            }
            // 设置输出音量
            codec->SetOutputVolume(volume);
            // 显示通知
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume/10));
        });

        // 设置音量减少按钮的长按事件
        volume_down_button_.OnLongPress([this]() {
            // 唤醒电源保存定时器
            power_save_timer_->WakeUp();
            // 设置输出音量为0
            GetAudioCodec()->SetOutputVolume(0);
            // 显示通知
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
        thing_manager.AddThing(iot::CreateThing("ESP32Temp"));

    }

public:
    XINGZHI_CUBE_0_96OLED_WIFI() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeIot();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging,float& esp32temp) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        esp32temp = power_manager_->GetTemperature();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(XINGZHI_CUBE_0_96OLED_WIFI);
