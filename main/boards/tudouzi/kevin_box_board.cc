#include "ml307_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "assets/lang_config.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

#define TAG "KevinBoxBoard"  // 定义日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

// KevinBoxBoard 类，继承自 Ml307Board
class KevinBoxBoard : public Ml307Board {
private:
    i2c_master_bus_handle_t display_i2c_bus_;  // 显示屏 I2C 总线句柄
    i2c_master_bus_handle_t codec_i2c_bus_;    // 音频编解码器 I2C 总线句柄
    Axp2101* axp2101_ = nullptr;               // AXP2101 电源管理芯片实例
    Button boot_button_;                        // 启动按钮
    Button volume_up_button_;                   // 音量增加按钮
    Button volume_down_button_;                 // 音量减少按钮
    PowerSaveTimer* power_save_timer_;          // 节能定时器

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(240, 60, -1);  // 创建节能定时器实例
        power_save_timer_->OnEnterSleepMode([this]() {        // 进入睡眠模式回调
            ESP_LOGI(TAG, "Enabling sleep mode");
            if (!modem_.Command("AT+MLPMCFG=\"sleepmode\",2,0")) {  // 启用模块睡眠模式
                ESP_LOGE(TAG, "Failed to enable module sleep mode");
            }

            auto display = GetDisplay();
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("sleepy");          // 设置表情为“睡眠”
            
            auto codec = GetAudioCodec();
            codec->EnableInput(false);              // 禁用音频输入
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 退出睡眠模式回调
            auto codec = GetAudioCodec();
            codec->EnableInput(true);               // 启用音频输入
            
            auto display = GetDisplay();
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("neutral");         // 设置表情为“中性”
        });
        power_save_timer_->SetEnabled(true);        // 启用节能定时器
    }

    // 启用 4G 模块
    void Enable4GModule() {
        // 配置 GPIO 为输出模式，并设置为高电平以启用 4G 模块
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 4),           // GPIO 4
            .mode = GPIO_MODE_OUTPUT,              // 输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE,       // 禁用中断
        };
        gpio_config(&ml307_enable_config);         // 配置 GPIO
        gpio_set_level(GPIO_NUM_4, 1);             // 设置 GPIO 为高电平
    }

    // 初始化显示屏 I2C 总线
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,            // I2C 端口 0
            .sda_io_num = DISPLAY_SDA_PIN,         // SDA 引脚
            .scl_io_num = DISPLAY_SCL_PIN,         // SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,     // 默认时钟源
            .glitch_ignore_cnt = 7,               // 毛刺忽略计数
            .intr_priority = 0,                   // 中断优先级
            .trans_queue_depth = 0,               // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,      // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));  // 创建 I2C 总线
    }

    // 初始化音频编解码器 I2C 总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,            // I2C 端口 1
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA 引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,    // 默认时钟源
            .glitch_ignore_cnt = 7,              // 毛刺忽略计数
            .intr_priority = 0,                  // 中断优先级
            .trans_queue_depth = 0,              // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,     // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 创建 I2C 总线
    }

    // 初始化按钮
    void InitializeButtons() {
        // 启动按钮按下事件
        boot_button_.OnPressDown([this]() {
            power_save_timer_->WakeUp();  // 唤醒设备
            Application::GetInstance().StartListening();  // 开始监听
        });
        // 启动按钮释放事件
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();  // 停止监听
        });

        // 音量增加按钮点击事件
        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;  // 增加音量
            if (volume > 100) {
                volume = 100;  // 限制最大音量
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        // 音量增加按钮长按事件
        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(100);  // 设置音量为最大值
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);  // 显示最大音量通知
        });

        // 音量减少按钮点击事件
        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;  // 减少音量
            if (volume < 0) {
                volume = 0;  // 限制最小音量
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        // 音量减少按钮长按事件
        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(0);  // 设置音量为 0（静音）
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);  // 显示静音通知
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();  // 获取物联网设备管理器实例
        thing_manager.AddThing(iot::CreateThing("Speaker"));    // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Battery"));    // 添加电池设备
    }

public:
    // 构造函数
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();  // 初始化显示屏 I2C 总线
        InitializeCodecI2c();    // 初始化音频编解码器 I2C 总线
        axp2101_ = new Axp2101(codec_i2c_bus_, AXP2101_I2C_ADDR);  // 创建 AXP2101 实例

        Enable4GModule();  // 启用 4G 模块

        InitializeButtons();        // 初始化按钮
        InitializePowerSaveTimer(); // 初始化节能定时器
        InitializeIot();           // 初始化物联网设备
    }

    // 获取 LED 实例
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 创建单 LED 实例
        return &led;
    }

    // 获取音频编解码器实例
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    // 获取显示屏实例
    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);
        return &display;
    }

    // 获取电池电量和充电状态
    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;

        charging = axp2101_->IsCharging();  // 获取充电状态
        if (charging != last_charging) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }

        level = axp2101_->GetBatteryLevel();  // 获取电池电量
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);  // 打印电池状态
        }

        static bool show_low_power_warning_ = false;
        if (axp2101_->IsDischarging()) {
            // 电量低于 10% 时，显示低电量警告
            if (!show_low_power_warning_ && level <= 10) {
                auto& app = Application::GetInstance();
                app.Alert(Lang::Strings::WARNING, Lang::Strings::BATTERY_LOW, "sad", Lang::Sounds::P3_VIBRATION);
                show_low_power_warning_ = true;
            }
            power_save_timer_->SetEnabled(true);  // 启用节能定时器
        } else {
            if (show_low_power_warning_) {
                auto& app = Application::GetInstance();
                app.DismissAlert();  // 关闭警告
                show_low_power_warning_ = false;
            }
            power_save_timer_->SetEnabled(false);  // 禁用节能定时器
        }
        return true;
    }
};

// 声明开发板实例
DECLARE_BOARD(KevinBoxBoard);