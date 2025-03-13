#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "settings.h"
#include "config.h"
#include "power_save_timer.h"
#include "font_awesome_symbols.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#define TAG "XminiC3Board"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_14_1);  // 声明字体
LV_FONT_DECLARE(font_awesome_14_1);  // 声明字体

// XminiC3Board 类继承自 WifiBoard，用于管理设备功能
class XminiC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C总线句柄
    Button boot_button_;  // 启动按钮
    bool press_to_talk_enabled_ = false;  // 长按说话模式是否启用
    PowerSaveTimer* power_save_timer_;  // 节能定时器

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(160, 60);  // 创建节能定时器
        power_save_timer_->OnEnterSleepMode([this]() {  // 进入睡眠模式回调
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            
            auto codec = GetAudioCodec();
            codec->EnableInput(false);  // 禁用音频输入
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 退出睡眠模式回调
            auto codec = GetAudioCodec();
            codec->EnableInput(true);  // 启用音频输入
            
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化音频编解码器的I2C总线
    void InitializeCodecI2c() {
        // 配置I2C总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化I2C总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 启动按钮点击回调
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
            }
            if (!press_to_talk_enabled_) {
                app.ToggleChatState();  // 切换聊天状态
            }
        });
        boot_button_.OnPressDown([this]() {  // 启动按钮按下回调
            power_save_timer_->WakeUp();  // 唤醒设备
            if (press_to_talk_enabled_) {
                Application::GetInstance().StartListening();  // 开始监听
            }
        });
        boot_button_.OnPressUp([this]() {  // 启动按钮释放回调
            if (press_to_talk_enabled_) {
                Application::GetInstance().StopListening();  // 停止监听
            }
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        Settings settings("vendor");  // 加载设置
        press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;  // 获取长按说话模式设置

        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("PressToTalk"));  // 添加长按说话设备
    }

public:
    // 构造函数，初始化设备
    XminiC3Board() : boot_button_(BOOT_BUTTON_GPIO) {  
        // 将ESP32C3的VDD SPI引脚作为普通GPIO口使用
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);

        InitializeCodecI2c();  // 初始化音频编解码器的I2C总线
        InitializeButtons();  // 初始化按钮
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeIot();  // 初始化物联网设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 创建LED对象
        return &led;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        static Ssd1306Display display(codec_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);  // 创建SSD1306显示对象
        return &display;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建ES8311音频编解码器对象
        return &audio_codec;
    }

    // 设置长按说话模式是否启用
    void SetPressToTalkEnabled(bool enabled) {
        press_to_talk_enabled_ = enabled;

        Settings settings("vendor", true);  // 保存设置
        settings.SetInt("press_to_talk", enabled ? 1 : 0);
        ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);  // 记录日志
    }

    // 获取长按说话模式是否启用
    bool IsPressToTalkEnabled() {
        return press_to_talk_enabled_;
    }
};

// 声明设备
DECLARE_BOARD(XminiC3Board);

// 物联网设备命名空间
namespace iot {

// PressToTalk 类继承自 Thing，用于管理长按说话模式
class PressToTalk : public Thing {
public:
    PressToTalk() : Thing("PressToTalk", "控制对话模式，一种是长按对话，一种是单击后连续对话。") {
        // 定义设备的属性
        properties_.AddBooleanProperty("enabled", "true 表示长按说话模式，false 表示单击说话模式", []() -> bool {
            auto board = static_cast<XminiC3Board*>(&Board::GetInstance());
            return board->IsPressToTalkEnabled();  // 获取长按说话模式状态
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetEnabled", "启用或禁用长按说话模式，调用前需要经过用户确认", ParameterList({
            Parameter("enabled", "true 表示长按说话模式，false 表示单击说话模式", kValueTypeBoolean, true)
        }), [](const ParameterList& parameters) {
            bool enabled = parameters["enabled"].boolean();
            auto board = static_cast<XminiC3Board*>(&Board::GetInstance());
            board->SetPressToTalkEnabled(enabled);  // 设置长按说话模式状态
        });
    }
};

} // namespace iot

// 声明物联网设备
DECLARE_THING(PressToTalk);