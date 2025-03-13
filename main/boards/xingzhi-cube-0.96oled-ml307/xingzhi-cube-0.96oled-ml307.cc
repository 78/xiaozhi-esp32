#include "ml307_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/ssd1306_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_save_timer.h"
#include "../xingzhi-cube-1.54tft-wifi/power_manager.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "XINGZHI_CUBE_0_96OLED_ML307"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_14_1);  // 声明字体
LV_FONT_DECLARE(font_awesome_14_1);  // 声明图标字体

// CustomDisplay类继承自Ssd1306Display，用于自定义显示功能
class CustomDisplay : public Ssd1306Display {
private:
    lv_obj_t* low_battery_popup_ = nullptr;  // 低电量弹出窗口对象

public:
    // 构造函数，初始化显示参数
    CustomDisplay(void* i2c_master_handle, int width, int height, bool mirror_x, bool mirror_y,
        const lv_font_t* text_font, const lv_font_t* icon_font)
        : Ssd1306Display(i2c_master_handle, width, height, mirror_x, mirror_y, text_font, icon_font) {
    }

    // 显示低电量弹出窗口
    void ShowLowBatteryPopup() {
        DisplayLockGuard lock(this);  // 加锁，确保线程安全

        if (low_battery_popup_ == nullptr) {
            // 创建弹出窗口
            low_battery_popup_ = lv_obj_create(lv_scr_act());
            lv_obj_set_size(low_battery_popup_, 120, 30);
            lv_obj_center(low_battery_popup_);
            lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
            lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    
            // 创建提示文本标签
            lv_obj_t* label = lv_label_create(low_battery_popup_);
            lv_label_set_text(label, "电量过低，请充电");
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_center(label);
        }
    
        // 显示弹出窗口
        lv_obj_clear_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }

    // 隐藏低电量弹出窗口
    void HideLowBatteryPopup() {
        DisplayLockGuard lock(this);  // 加锁，确保线程安全
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    }
};

// XINGZHI_CUBE_0_96OLED_ML307类继承自Ml307Board，用于管理硬件功能
class XINGZHI_CUBE_0_96OLED_ML307 : public Ml307Board {
private:
    i2c_master_bus_handle_t display_i2c_bus_;  // I2C总线句柄
    Button boot_button_;  // 启动按钮
    Button volume_up_button_;  // 音量增加按钮
    Button volume_down_button_;  // 音量减少按钮
    CustomDisplay* display_;  // 自定义显示对象
    PowerSaveTimer* power_save_timer_;  // 节能定时器
    PowerManager power_manager_;  // 电源管理器
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;  // LCD面板IO句柄
    esp_lcd_panel_handle_t panel_ = nullptr;  // LCD面板句柄

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_21);  // 初始化RTC GPIO
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);  // 设置GPIO方向
        rtc_gpio_set_level(GPIO_NUM_21, 1);  // 设置GPIO电平

        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);  // 创建节能定时器
        power_save_timer_->OnEnterSleepMode([this]() {  // 进入睡眠模式回调
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 退出睡眠模式回调
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
        });
        power_save_timer_->OnShutdownRequest([this]() {  // 关机请求回调
            ESP_LOGI(TAG, "Shutting down");
            rtc_gpio_set_level(GPIO_NUM_21, 0);
            rtc_gpio_hold_en(GPIO_NUM_21);  // 启用保持功能，确保睡眠期间电平不变
            esp_lcd_panel_disp_on_off(panel_, false);  // 关闭显示
            esp_deep_sleep_start();  // 进入深度睡眠
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化显示I2C总线
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));  // 创建I2C总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 启动按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto& app = Application::GetInstance();
            app.ToggleChatState();  // 切换聊天状态
        });

        volume_up_button_.OnClick([this]() {  // 音量增加按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;  // 增加音量
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_up_button_.OnLongPress([this]() {  // 音量增加按钮长按回调
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(100);  // 设置最大音量
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);  // 显示最大音量通知
        });

        volume_down_button_.OnClick([this]() {  // 音量减少按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;  // 减少音量
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_down_button_.OnLongPress([this]() {  // 音量减少按钮长按回调
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(0);  // 静音
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);  // 显示静音通知
        });
    }

    // 初始化IoT设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Battery"));  // 添加电池设备
    }

public:
    // 构造函数，初始化硬件
    XINGZHI_CUBE_0_96OLED_ML307() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        power_manager_(GPIO_NUM_38) {
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeDisplayI2c();  // 初始化显示I2C总线
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化IoT设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        static CustomDisplay display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);
        return &display;
    }

    // 获取电池电量和充电状态
    virtual bool GetBatteryLevel(int& level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;

        charging = power_manager_.IsCharging();  // 获取充电状态
        if (charging != last_charging) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }

        level = power_manager_.ReadBatteryLevel(charging != last_charging);  // 获取电池电量
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);  // 记录日志
        }

        static bool show_low_power_warning_ = false;
        if (power_manager_.IsBatteryLevelSteady()) {
            if (!charging) {
                // 电量低于 15% 时，显示低电量警告
                if (!show_low_power_warning_ && level <= 15) {
                    display_->ShowLowBatteryPopup();  // 显示低电量弹出窗口
                    show_low_power_warning_ = true;
                }
                power_save_timer_->SetEnabled(true);  // 启用节能定时器
            } else {
                if (show_low_power_warning_) {
                    display_->HideLowBatteryPopup();  // 隐藏低电量弹出窗口
                    show_low_power_warning_ = false;
                }
                power_save_timer_->SetEnabled(false);  // 禁用节能定时器
            }
        }
        return true;
    }

    // 设置节能模式
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }
        Ml307Board::SetPowerSaveMode(enabled);  // 调用父类方法
    }
};

// 声明板子对象
DECLARE_BOARD(XINGZHI_CUBE_0_96OLED_ML307);