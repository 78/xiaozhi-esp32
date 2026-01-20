#include "wifi_board.h"
#include "config.h"
#include "power_manager.h"
#include "led_controller.h"
#include "servo_controller.h"
#include "codecs/es8389_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "led/circular_strip.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include "device_state_machine.h"
#include "mcp_tools.h"
#include <esp_log.h>
#include <driver/rtc_io.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#define TAG "NanoZhumian"

class NanoZhumian : public WifiBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    FogSeekPowerManager power_manager_;
    FogSeekLedController led_controller_;
    FogSeekServoController servo_controller_;
    CircularStrip *rgb_led_strip_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    AudioCodec *audio_codec_ = nullptr;
    esp_timer_handle_t check_idle_timer_ = nullptr;

    // 初始化I2C外设
    void InitializeI2c()
    {
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

    // 初始化电源管理器
    void InitializePowerManager()
    {
        power_pin_config_t power_pin_config = {
            .hold_gpio = PWR_HOLD_GPIO,
            .charging_gpio = PWR_CHARGING_GPIO,
            .charge_done_gpio = PWR_CHARGE_DONE_GPIO,
            .adc_gpio = BATTERY_ADC_GPIO};
        power_manager_.Initialize(&power_pin_config);
    }

    // 初始化LED控制器
    void InitializeLedController()
    {
        led_pin_config_t led_pin_config = {
            .red_gpio = LED_RED_GPIO,
            .green_gpio = LED_GREEN_GPIO};
        led_controller_.InitializeLeds(power_manager_, &led_pin_config);

        // 初始化RGB灯带
        rgb_led_strip_ = new CircularStrip((gpio_num_t)LED_RGB_GPIO, 8);
    }

    // 初始化舵机控制器
    void InitializeServoController()
    {
        // 使用配置文件中定义的舵机控制引脚 (GPIO_NUM_5)
        servo_controller_.Initialize(SERVO_BODY_GPIO);

        // 设置舵机初始位置
        servo_controller_.SetAngle(90); // 90度位置（中间）

        ESP_LOGI(TAG, "Servo controller initialized on GPIO %d.", SERVO_BODY_GPIO);
    }

    // 初始化音频功放引脚并默认关闭功放
    void InitializeAudioAmplifier()
    {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        SetAudioAmplifierState(false); // 默认关闭功放
    }

    // 设置音频功放状态
    void SetAudioAmplifierState(bool enable)
    {
        gpio_set_level(AUDIO_CODEC_PA_PIN, enable ? 1 : 0);
    }

    // 初始化扩展板电源使能引脚
    void InitializeExtensionPowerEnable()
    {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << EXT_POWER_ENABLE_GPIO);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        SetExtensionPowerEnableState(false); // 默认关闭扩展板电源使能
    }

    // 设置扩展板电源使能状态
    void SetExtensionPowerEnableState(bool enable)
    {
        gpio_set_level(EXT_POWER_ENABLE_GPIO, enable ? 1 : 0);
    }

    // 初始化按键回调
    void InitializeButtonCallbacks()
    {
        ctrl_button_.OnClick([this]()
                             {
                                 servo_controller_.SetAngle(45);
                                 // 延时500ms后返回到90度位置
                                 vTaskDelay(pdMS_TO_TICKS(500));
                                 servo_controller_.SetAngle(90);

                                 // 循环切换RGB灯带颜色
                                 static int color_index = 0;
                                 switch (color_index)
                                 {
                                 case 0:
                                     rgb_led_strip_->SetAllColor({255, 0, 255}); // 紫色
                                     break;
                                 case 1:
                                     rgb_led_strip_->SetAllColor({0, 255, 0}); // 绿色
                                     break;
                                 case 2:
                                     rgb_led_strip_->SetAllColor({255, 255, 0}); // 黄色
                                     break;
                                 case 3:
                                     rgb_led_strip_->SetAllColor({0, 0, 255}); // 蓝色
                                     break;
                                 case 4:
                                     rgb_led_strip_->SetAllColor({255, 165, 0}); // 橙色
                                     break;
                                 case 5:
                                     rgb_led_strip_->SetAllColor({0, 255, 255}); // 青色
                                     break;
                                 default:
                                     rgb_led_strip_->SetAllColor({255, 255, 255}); // 白色
                                     break;
                                 }
                                 color_index = (color_index + 1) % 7; // 循环使用7种颜色

                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 切换聊天状态（打断）
                             });
        ctrl_button_.OnDoubleClick([this]()
                                   {
                                    rgb_led_strip_->SetAllColor({0, 0, 0}); // 关灯
                                    auto &app = Application::GetInstance();
                                    if (app.GetDeviceState() == kDeviceStateStarting)
                                    {
                                        EnterWifiConfigMode();
                                        return;
                                    } });
        ctrl_button_.OnLongPress([this]()
                                 {
            // 切换电源状态
            if (!power_manager_.IsPowerOn()) {
                PowerOn();
            } else {
                PowerOff();
            } });
    }

    // 处理自动唤醒逻辑
    void HandleAutoWake()
    {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            auto &app = Application::GetInstance();
            // USB充电状态下开机需要播放音效
            if (power_manager_.IsUsbPowered())
            {
                app.PlaySound(Lang::Sounds::OGG_SUCCESS);
                vTaskDelay(pdMS_TO_TICKS(500)); // 延时500ms播放音效
            }
            app.Schedule([]()
                         {
                            auto &app = Application::GetInstance();
                            app.ToggleChatState(); });
        }
        else
        {
            // 设备尚未进入空闲状态，500ms后再次检查，使用定时器异步检查，不阻塞当前任务
            esp_timer_handle_t check_timer;
            esp_timer_create_args_t timer_args = {};
            timer_args.callback = [](void *arg)
            {
                auto instance = static_cast<NanoZhumian *>(arg);
                instance->HandleAutoWake();
            };
            timer_args.arg = this;
            timer_args.name = "check_idle_timer";
            esp_timer_create(&timer_args, &check_timer);
            esp_timer_start_once(check_timer, 500000); // 500ms = 500000微秒
        }
    }

    // 开机流程
    void PowerOn()
    {
        power_manager_.PowerOn();                        // 更新电源状态
        led_controller_.UpdateLedStatus(power_manager_); // 更新LED灯状态

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(70); // 开机后将音量设置为默认值
        SetAudioAmplifierState(true);

        SetExtensionPowerEnableState(true); // 开机时打开扩展板电源使能

        ESP_LOGI(TAG, "Device powered on.");

        HandleAutoWake(); // 开机自动唤醒
    }

    // 关机流程
    void PowerOff()
    {
        SetExtensionPowerEnableState(false); // 关机时关闭扩展板电源使能
        rgb_led_strip_->SetAllColor({0, 0, 0});

        power_manager_.PowerOff();
        led_controller_.UpdateLedStatus(power_manager_);

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(0); // 关机后将音量设置为默0
        SetAudioAmplifierState(false);

        Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle); // 关机后将设备状态设置为空闲，便于下次开机自动唤醒

        ESP_LOGI(TAG, "Device powered off.");
    }

    // 初始化MCP工具
    void InitializeMCP()
    {
        // 获取MCP服务器实例
        auto &mcp_server = McpServer::GetInstance();

        // 初始化RGB LED MCP 工具
        InitializeRgbLedMCP(mcp_server, rgb_led_strip_);

        // 初始化系统级MCP工具（如关机功能）
        InitializeSystemMCP(mcp_server, power_manager_);
    }

public:
    NanoZhumian() : boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializePowerManager();
        InitializeLedController();
        InitializeAudioAmplifier();
        InitializeExtensionPowerEnable();
        InitializeButtonCallbacks();
        InitializeMCP();
        InitializeServoController();

        // 设置电源状态变化回调函数
        power_manager_.SetPowerStateCallback([this](FogSeekPowerManager::PowerState state)
                                             { led_controller_.UpdateLedStatus(power_manager_); });
    }

    virtual Led *GetLed() override
    {
        return led_controller_.GetGreenLed();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8389AudioCodec audio_codec(
            i2c_bus_,
            (i2c_port_t)0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC,
            AUDIO_CODEC_ES8389_ADDR,
            true,
            true);
        return &audio_codec;
    }

    ~NanoZhumian()
    {
        if (i2c_bus_)
        {
            i2c_del_master_bus(i2c_bus_);
        }

        // 删除RGB灯带对象
        if (rgb_led_strip_)
        {
            delete rgb_led_strip_;
            rgb_led_strip_ = nullptr;
        }
    }
};

DECLARE_BOARD(NanoZhumian);