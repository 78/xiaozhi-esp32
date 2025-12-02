#include "dual_network_board.h"
#include "config.h"
#include "power_manager.h"
#include "display_manager.h"
#include "led_controller.h"
#include "codecs/es8389_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#define TAG "FogSeekEdgeEs8389"

class FogSeekEdgeEs8389 : public DualNetworkBoard
{
private:
    Button boot_button_;
    Button ctrl_button_;
    FogSeekPowerManager power_manager_;
    FogSeekDisplayManager display_manager_;
    FogSeekLedController led_controller_;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    AudioCodec *audio_codec_ = nullptr;

    // 添加自动唤醒标志位
    bool auto_wake_flag_ = false;

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
    }

    // 初始化显示管理器
    void InitializeDisplayManager()
    {
        lcd_pin_config_t lcd_pin_config = {
            .io0_gpio = LCD_IO0_GPIO,
            .io1_gpio = LCD_IO1_GPIO,
            .scl_gpio = LCD_SCL_GPIO,
            .io2_gpio = LCD_IO2_GPIO,
            .io3_gpio = LCD_IO3_GPIO,
            .cs_gpio = LCD_CS_GPIO,
            .dc_gpio = LCD_DC_GPIO,
            .reset_gpio = LCD_RESET_GPIO,
            .im0_gpio = LCD_IM0_GPIO,
            .im2_gpio = LCD_IM2_GPIO,
            .bl_gpio = LCD_BL_GPIO,
            .width = LCD_H_RES,
            .height = LCD_V_RES,
            .offset_x = DISPLAY_OFFSET_X,
            .offset_y = DISPLAY_OFFSET_Y,
            .mirror_x = DISPLAY_MIRROR_X,
            .mirror_y = DISPLAY_MIRROR_Y,
            .swap_xy = DISPLAY_SWAP_XY};
        display_manager_.Initialize(BOARD_LCD_TYPE, &lcd_pin_config);
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

    // 初始化按键回调
    void InitializeButtonCallbacks()
    {
        ctrl_button_.OnPressDown([this]()
                                 {
                                     led_controller_.SetPrePowerOnState(true); // 按键按下时设置预开机标志位
                                 });
        ctrl_button_.OnPressUp([this]()
                               {
                                   led_controller_.SetPrePowerOnState(false); // 按键松开时清除预开机标志位
                               });

        ctrl_button_.OnClick([this]()
                             {
                                 auto &app = Application::GetInstance();
                                 app.ToggleChatState(); // 切换聊天状态（打断）
                             });
        ctrl_button_.OnDoubleClick([this]()
                                   {
                                      // 停止当前网络连接并进入配网模式
                                      auto &wifi_station = WifiStation::GetInstance();
                                      wifi_station.Stop();
                                      wifi_config_mode_ = true;
                                      EnterWifiConfigMode(); });
        ctrl_button_.OnLongPress([this]()
                                 {
                                     // 切换电源状态
                                     if (!power_manager_.IsPowerOn()) {
                                     PowerOn();
                                     } else {
                                     PowerOff();
                                     } });
    }

    // 开机流程
    void PowerOn()
    {
        power_manager_.PowerOn();
        led_controller_.SetPowerState(true);
        led_controller_.UpdateBatteryStatus(power_manager_);
        // display_manager_.SetBrightness(100);
        SetAudioAmplifierState(true);

        // 开机自动唤醒
        auto_wake_flag_ = true;
        OnDeviceStateChanged(DeviceState::kDeviceStateUnknown,
                             Application::GetInstance().GetDeviceState());

        ESP_LOGI(TAG, "Device powered on.");
    }

    // 关机流程
    void PowerOff()
    {
        power_manager_.PowerOff();
        led_controller_.SetPowerState(false);
        led_controller_.UpdateBatteryStatus(power_manager_);
        // display_manager_.SetBrightness(0);
        SetAudioAmplifierState(false);

        // 重置自动唤醒标志位到默认状态
        auto_wake_flag_ = false;
        Application::GetInstance().SetDeviceState(DeviceState::kDeviceStateIdle);

        ESP_LOGI(TAG, "Device powered off.");
    }

    // 处理自动唤醒逻辑
    void HandleAutoWake(DeviceState current_state)
    {
        // 检查是否需要自动唤醒
        if (auto_wake_flag_ && current_state == DeviceState::kDeviceStateIdle)
        {
            auto_wake_flag_ = false; // 关闭标志位

            auto &app = Application::GetInstance();
            // USB供电需要播放音效
            if (power_manager_.IsUsbPowered())
                app.PlaySound(Lang::Sounds::OGG_SUCCESS);

            vTaskDelay(pdMS_TO_TICKS(500)); // 添加延时确保声音播放完成
            // 进入聆听状态
            app.Schedule([]()
                         {
            auto &app = Application::GetInstance();
    app.ToggleChatState(); });
        }
    }

    // 设备状态变更处理函数
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state)
    {
        // 只有在设备开机状态下才处理LED和显示屏状态
        if (power_manager_.IsPowerOn())
        {
            led_controller_.HandleDeviceState(current_state, power_manager_);
            display_manager_.HandleDeviceState(current_state);

            // 处理自动唤醒逻辑
            HandleAutoWake(current_state);
        }
    }

    // 电源状态变更处理函数，用于关机充电时，充电状态变化更新指示灯
    void OnPowerStateChanged(FogSeekPowerManager::PowerState state)
    {
        if (!power_manager_.IsPowerOn() || Application::GetInstance().GetDeviceState() == DeviceState::kDeviceStateIdle)
        {
            led_controller_.UpdateBatteryStatus(power_manager_);
        }
    }

    // 启用4G模块
    // void Enable4GModule()
    // {
    //     // 配置4G模块的控制引脚
    //     gpio_config_t ml307_enable_config = {
    //         .pin_bit_mask = (1ULL << 45), // 使用GPIO45控制4G模块
    //         .mode = GPIO_MODE_OUTPUT,
    //         .pull_up_en = GPIO_PULLUP_DISABLE,
    //         .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //         .intr_type = GPIO_INTR_DISABLE,
    //     };
    //     gpio_config(&ml307_enable_config);
    //     gpio_set_level(GPIO_NUM_45, 1);
    // }

public:
    FogSeekEdgeEs8389() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN),
                          boot_button_(BOOT_BUTTON_GPIO), ctrl_button_(CTRL_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeButtonCallbacks();
        InitializePowerManager();
        InitializeLedController();
        // InitializeDisplayManager();
        InitializeAudioAmplifier();
        // Enable4GModule(); // 启用4G模块

        // 设置电源状态变化回调函数
        power_manager_.SetPowerStateCallback([this](FogSeekPowerManager::PowerState state)
                                             { OnPowerStateChanged(state); });

        // 注册设备交互状态变更回调
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state)
                                                                           { OnDeviceStateChanged(previous_state, current_state); });
    }

    // virtual Display *GetDisplay() override
    // {
    //     return display_manager_.GetDisplay();
    // }

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
            true);
        return &audio_codec;
    }

    ~FogSeekEdgeEs8389()
    {
        if (i2c_bus_)
        {
            i2c_del_master_bus(i2c_bus_);
        }
    }
};

DECLARE_BOARD(FogSeekEdgeEs8389);