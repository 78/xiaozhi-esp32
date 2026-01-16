#include "wifi_board.h"
#include "codecs/es8389_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "led/gpio_led.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "mcp_server.h"         // 添加这行
#include "pwm_led_controller.h" // 新增：引入 PWM LED 控制器头文件
#include "uart_comm.h"
#include "ec11_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include <esp_rom_sys.h>
#include <esp_sleep.h>
#include "assets/lang_config.h"

#define TAG "ai_martube_esp32s3"
// 旋转编码器轻量去抖窗口（微秒）
#define ENCODER_DEBOUNCE_US 20000
#define ENCODER_WINDOW_US 50000
#define CHARGE_DEBOUNCE_US 2000000

// 按键事件类型
enum KeyEventType {
    KEY_EVENT_PRESS = 0,
    KEY_EVENT_RELEASE = 1
};

// 按键事件结构
struct KeyEvent {
    KeyEventType type;
    int64_t timestamp;
};

class XL9555 : public I2cDevice
{
public:
    XL9555(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
    {
        WriteReg(0x06, 0x03);
        WriteReg(0x07, 0xF0);
    }

    void SetOutputState(uint8_t bit, uint8_t level)
    {
        uint16_t data;
        int index = bit;

        if (bit < 8)
        {
            data = ReadReg(0x02);
        }
        else
        {
            data = ReadReg(0x03);
            index -= 8;
        }

        data = (data & ~(1 << index)) | (level << index);

        if (bit < 8)
        {
            WriteReg(0x02, data);
        }
        else
        {
            WriteReg(0x03, data);
        }
    }
};

class ai_martube_esp32s3 : public WifiBoard
{
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button shutdown_button_;
    Button key_input_button_;
    LcdDisplay *display_;
    XL9555 *xl9555_;
    Esp32Camera *camera_;
    GpioLed *pwm_led_;                     // 添加PWM LED成员变量
    esp_timer_handle_t volume_timer_ = nullptr; // 音量调节去抖定时器
    PwmLedController *pwm_led_controller_; // 新增：PWM LED 控制器指针
    UartComm *uart_comm_;                  // 新增：串口通信成员
    esp_timer_handle_t boot_breath_timer_ = nullptr;
    bool boot_breathing_ = false;
    uint8_t breath_min_ = 5;
    uint8_t breath_max_ = 100;
    uint8_t breath_current_ = 5;
    bool breath_up_ = true;
    int breath_interval_ms_ = 30;
    int breath_step_ = 2;

    bool to_open_audio = false;
    
    // 按键相关变量
    uint8_t light_mode_ = 2;  // 灯光模式：0=灭, 1=30%亮度, 2=100%亮度

    // 旋转编码器相关变量
    // 编码器实例
    Ec11Encoder* encoder_ = nullptr;
    int current_volume_;  // 当前音量 (0-100)


    
    // 模式标志：false=AI模式，true=蓝牙模式
    bool bluetooth_mode_ = false;

    // 电池电量相关变量
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    float battery_voltage_;
    float power_ref_voltage_;
    int battery_percentage_;
    int64_t last_battery_check_time_;
    
    // 电池提醒状态变量
    bool battery_remind_first_triggered_;  // 3.3V 提醒已触发
    bool battery_remind_second_triggered_; // 2.85V 提醒已触发
    bool battery_remind_end_triggered_;  // 2.8V 提醒已触发
    bool shutdown_in_progress_;           // 关机进行中
    int last_charge_level_ = -1;
    int charge_candidate_level_ = -1;
    int64_t charge_candidate_start_us_ = 0;

    void InitializeI2c()
    {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0,
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

        // Initialize XL9555
        // xl9555_ = new XL9555(i2c_bus_, 0x20);
    }

    static void BootBreathTimerCallback(void* arg)
    {
        auto* self = static_cast<ai_martube_esp32s3*>(arg);
        self->HandleBootBreathTick();
    }

    void HandleBootBreathTick()
    {
        if (!boot_breathing_ || pwm_led_ == nullptr) {
            if (boot_breath_timer_) {
                esp_timer_stop(boot_breath_timer_);
            }
            return;
        }

        uint8_t cur = breath_current_;
        if (breath_up_) {
            if (cur + breath_step_ >= breath_max_) {
                cur = breath_max_;
                breath_up_ = false;
            } else {
                cur = cur + breath_step_;
            }
        } else {
            if (cur <= breath_min_ + breath_step_) {
                cur = breath_min_;
                breath_up_ = true;
            } else {
                cur = cur - breath_step_;
            }
        }
        breath_current_ = cur;
        pwm_led_->SetBrightness(cur);
        pwm_led_->TurnOn();
    }

    // Initialize spi peripheral
    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = LCD_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = LCD_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
        {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); 
        });

        shutdown_button_.OnLongPress([this]()
        {
            ESP_LOGI(TAG, "Shutdown command triggered by long press");
            // 设备状态设置为待机
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() != kDeviceStateIdle)  {
                app.SetDeviceState(kDeviceStateIdle);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            // 提醒用户关机
            gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
            gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_ENABLE_LEVEL);
            app.PlaySound(Lang::Sounds::OGG_POWEROFF);
            vTaskDelay(pdMS_TO_TICKS(1000));
            gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_DISABLE_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(100));
            SetPowerState(false);
        });

        shutdown_button_.OnClick([this]()
        {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Shutdown short press: abort speaking and enter listening");
                app.ToggleChatState();
            } else {
                ESP_LOGI(TAG, "Shutdown short press ignored: current state not speaking");
            }
        });
        
        key_input_button_.OnClick([this]()
        {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            else
            {
                light_mode_ = (light_mode_ + 1) % 3;  // 0->1->2->0
                ESP_LOGI(TAG, "Short press: switch light mode to %u", light_mode_);
                SetLightMode(light_mode_);
            }
        });

        key_input_button_.OnLongPress([this]()
        {
            auto& app = Application::GetInstance();
            // 长切换模式：AI 模式 <-> 蓝牙模式，并发送串口命令
            // AI -> 蓝牙：A5 00 02 05
            // 蓝牙 -> AI：A5 00 02 06
            if (!bluetooth_mode_) {
                // 设备状态设置为待机
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateSpeaking) {
                    app.AbortSpeaking(kAbortReasonNone);
                }
                app.SetDeviceState(kDeviceStateIdle);

                // 提醒用户切换到蓝牙模式
                app.PlaySound(Lang::Sounds::OGG_BTMODE);
                vTaskDelay(pdMS_TO_TICKS(2000));

                
                // 切到蓝牙模式
                gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x05};
                if (uart_comm_ && uart_comm_->IsReady()) {
                    uart_comm_->Send(cmd, sizeof(cmd));
                } else {
                    ESP_LOGW(TAG, "UART not ready, skip sending BT command");
                }
                ESP_LOGI(TAG, "Long press: switch to Bluetooth mode, sent A5 00 02 07");
                bluetooth_mode_ = true;
            } else {
                // 提醒用户切换到 AI 模式
                gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                app.PlaySound(Lang::Sounds::OGG_AIMODE);
                vTaskDelay(pdMS_TO_TICKS(2000));

                
                // 切到 AI 模式
                gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x06};
                if (uart_comm_ && uart_comm_->IsReady()) {
                    uart_comm_->Send(cmd, sizeof(cmd));
                } else {
                    ESP_LOGW(TAG, "UART not ready, skip sending AI command");
                }
                ESP_LOGI(TAG, "Long press: switch to AI mode, sent A5 00 02 08");
                bluetooth_mode_ = false;
                std::string wake_word = "你好小王子";
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                auto& app = Application::GetInstance();
                app.WakeWordInvoke(wake_word);
            }
        });

    }


    // 初始化3个音频切换IO(SWITCH_INPUT_GPIO、SWITCH_OUTPUT_GPIO、AUDIO_CODEC_PA_GPIO)：配置为输出模式，上电后设置为高电平（ESP32S3使用）
    void InitializeAudioSwitch()
    {
        gpio_config_t audio_switch_init_struct = {0};

        audio_switch_init_struct.mode = GPIO_MODE_OUTPUT;
        audio_switch_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        audio_switch_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        audio_switch_init_struct.pin_bit_mask = 1ull << SWITCH_INPUT_GPIO | 1ull << SWITCH_OUTPUT_GPIO | 1ull << AUDIO_CODEC_PA_GPIO;
        ESP_ERROR_CHECK(gpio_config(&audio_switch_init_struct));
        
        // 上电后设置为高电平，使用蓝牙音频
        gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
        gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
        gpio_set_level(AUDIO_CODEC_PA_GPIO, 1);
        
    }


    // 初始化电机控制IO：将GPIO配置为输出模式，初始状态为关闭电机
    void InitializeMotor()
    {
        gpio_config_t motor_init_struct = {0};

        motor_init_struct.mode = GPIO_MODE_OUTPUT;
        motor_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        motor_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        motor_init_struct.pin_bit_mask = 1ull << MOTOR_CONTROL_GPIO;
        ESP_ERROR_CHECK(gpio_config(&motor_init_struct));
        gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_DISABLE_LEVEL);
    }
    



    // 控制灯光模式
    void SetLightMode(uint8_t mode)
    {

        if (pwm_led_controller_ && pwm_led_controller_->IsReady()) {
            switch (mode) {
                case 0:  // 关闭
                    pwm_led_controller_->TurnOff();
                    ESP_LOGI(TAG, "Light mode: OFF");
                    break;
                case 1:  // 30% 亮度
                    pwm_led_controller_->SetBrightnessPercent(30);
                    ESP_LOGI(TAG, "Light mode: 30%% brightness");
                    break;
                case 2:  // 100% 亮度
                    pwm_led_controller_->SetBrightnessPercent(100);
                    ESP_LOGI(TAG, "Light mode: 100%% brightness");
                    break;
                default:
                    ESP_LOGW(TAG, "Invalid light mode: %d", mode);
                    return;
            }
            light_mode_ = mode;
        }
    }

    // 自定义输入低电平事件：蓝牙模式播放音乐
    void OnCustomInputLowEvent()
    {
        gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_ENABLE_LEVEL);
        if (!bluetooth_mode_) return;
        uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x13};
        if (uart_comm_ && uart_comm_->IsReady()) {
            uart_comm_->Send(cmd, sizeof(cmd));
        } else {
            ESP_LOGW(TAG, "UART not ready, skip sending BT command");
        }
    }

    // 自定义输入高电平事件：蓝牙模式暂停音乐
    void OnCustomInputHighEvent()
    {
        gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_DISABLE_LEVEL);
        if (!bluetooth_mode_) return;
        uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x14};
        if (uart_comm_ && uart_comm_->IsReady()) {
            uart_comm_->Send(cmd, sizeof(cmd));
        } else {
            ESP_LOGW(TAG, "UART not ready, skip sending BT command");
        }
    }


    // 初始化电源开机控制IO：配置为输出模式，初始状态为高电平（开机）
    void InitializePowerControl()
    {
        gpio_config_t power_init_struct = {0};

        power_init_struct.mode = GPIO_MODE_OUTPUT;
        power_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        power_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        power_init_struct.pin_bit_mask = 1ull << POWER_ON_CONTROL_GPIO;
        ESP_ERROR_CHECK(gpio_config(&power_init_struct));
        
        // 设置为高电平（开机状态）
        gpio_set_level(POWER_ON_CONTROL_GPIO, 1);

        ESP_LOGI(TAG, "Power control initialized on GPIO %d, status: HIGH (ON)", POWER_ON_CONTROL_GPIO);
    }

    void InitializeChargeStatus()
    {
        gpio_config_t in_cfg = {0};
        in_cfg.mode = GPIO_MODE_INPUT;
        in_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        in_cfg.pin_bit_mask = 1ull << CHARGE_INPUT_GPIO;
        ESP_ERROR_CHECK(gpio_config(&in_cfg));

        gpio_config_t led_cfg = {0};
        led_cfg.mode = GPIO_MODE_OUTPUT;
        led_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        led_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_cfg.pin_bit_mask = 1ull << CHARGE_STATUS_LED_GPIO;
        ESP_ERROR_CHECK(gpio_config(&led_cfg));

        int level = gpio_get_level(CHARGE_INPUT_GPIO);
        last_charge_level_ = level;
        gpio_set_level(CHARGE_STATUS_LED_GPIO, level ? 1 : 0);
        ESP_LOGI(TAG, "Charge init: in=%d, led=%d", level, level ? 1 : 0);
        charge_candidate_level_ = level;
        charge_candidate_start_us_ = esp_timer_get_time();
    }

    void UpdateChargeStatus()
    {
        int64_t now = esp_timer_get_time();
        int level = gpio_get_level(CHARGE_INPUT_GPIO);
        if (level != charge_candidate_level_) {
            charge_candidate_level_ = level;
            charge_candidate_start_us_ = now;
        }
        if (charge_candidate_level_ != last_charge_level_) {
            if (now - charge_candidate_start_us_ >= CHARGE_DEBOUNCE_US) {
                last_charge_level_ = charge_candidate_level_;
                gpio_set_level(CHARGE_STATUS_LED_GPIO, last_charge_level_ ? 1 : 0);
                ESP_LOGI(TAG, "Charge state changed: %d", last_charge_level_);
            }
        } else {
            gpio_set_level(CHARGE_STATUS_LED_GPIO, last_charge_level_ ? 1 : 0);
        }
    }

    // 初始化自定义输入IO：配置为输入模式，启用上拉
    void InitializeCustomInput()
    {
        gpio_config_t custom_input_init_struct = {0};

        custom_input_init_struct.mode = GPIO_MODE_INPUT;
        custom_input_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        custom_input_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        custom_input_init_struct.pin_bit_mask = 1ull << CUSTOM_INPUT_GPIO;
        ESP_ERROR_CHECK(gpio_config(&custom_input_init_struct));
    }

    // 电源控制函数
    void SetPowerState(bool power_on)
    {
        gpio_set_level(POWER_ON_CONTROL_GPIO, power_on ? 1 : 0);
        ESP_LOGI(TAG, "Power control set to %s", power_on ? "HIGH (ON)" : "LOW (OFF)");
    }


    // 初始化旋转编码器：配置为输入模式，启用上拉
    void InitializeEncoder()
    {      
        if (!encoder_) {
            // 创建音量调节去抖定时器
            esp_timer_create_args_t volume_timer_args = {
                .callback = [](void* arg) {
                    auto* self = static_cast<ai_martube_esp32s3*>(arg);
                    auto& app = Application::GetInstance();
                    if (self->current_volume_ >= 100) {
                        app.PlaySound(Lang::Sounds::OGG_MAXSOUND);
                    } else {
                        app.PlaySound(Lang::Sounds::OGG_SOUNDSET);
                    }
                },
                .arg = this,
                .name = "volume_timer"
            };
            ESP_ERROR_CHECK(esp_timer_create(&volume_timer_args, &volume_timer_));

            // 初始化旋钮：GPIO A/B
            encoder_ = new Ec11Encoder(ENCODER_A_GPIO, ENCODER_B_GPIO);
            encoder_->SetCallback([this](int step) {
                // 仅发送简单事件，避免阻塞定时器回调
                ESP_LOGI(TAG, "Knob event: %d", step);
                if (bluetooth_mode_) {
                    uint8_t cmd_up[4] = {0xA5, 0x00, 0x02, 0x01};
                    uint8_t cmd_down[4] = {0xA5, 0x00, 0x02, 0x02};
                    if (step > 0) {
                        if (uart_comm_ && uart_comm_->IsReady()) {
                            uart_comm_->Send(cmd_up, sizeof(cmd_up));
                        }
                    } else {
                        if (uart_comm_ && uart_comm_->IsReady()) {
                            uart_comm_->Send(cmd_down, sizeof(cmd_down));
                        }
                    }
                } else {
 
                    int delta = step * 5;
                    int v = current_volume_ + delta;
                    bool hit_max = false;

                    if (v > 100) {
                        v = 100;
                        hit_max = true;
                    }
                    if (v < 0) v = 0;

                    if (v != current_volume_) {
                        OnVolumeChange(v);
                        esp_timer_stop(volume_timer_);
                        esp_timer_start_once(volume_timer_, 200000);
                    } else if (hit_max && delta > 0) {
                         esp_timer_stop(volume_timer_);
                         esp_timer_start_once(volume_timer_, 200000);
                    }
                }

            });
            encoder_->Start();
        }
        
    }



    // 音量变化处理
    void OnVolumeChange(int volume)
    {
        ESP_LOGI(TAG, "Volume changed to %d%%", volume);
        
        // 获取音频编解码器并设置音量
        auto codec = GetAudioCodec();
        if (codec) {
            codec->SetOutputVolume(volume);
        }
        
        // 更新内部音量状态
        current_volume_ = volume;
    }

    // 初始化电池电量检测
    void InitializeBatteryMonitor()
    {
        // 配置ADC1
        adc_oneshot_unit_init_cfg_t init_config1 = {};
        init_config1.unit_id = ADC_UNIT_1;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle_));

        // 配置ADC通道
        adc_oneshot_chan_cfg_t config = {};
        config.bitwidth = ADC_BITWIDTH_12;
        config.atten = ADC_ATTEN_DB_12;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, BATTERY_ADC_CHANNEL, &config));
        // 2.5V参考电压通道
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, POWER_ADC_CHANNEL, &config));

        // 校准ADC
        adc_cali_curve_fitting_config_t cali_config = {};
        cali_config.unit_id = ADC_UNIT_1;
        cali_config.atten = ADC_ATTEN_DB_12;
        cali_config.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_));
        
        // 初始化电池状态
        battery_voltage_ = 0.0f;
        power_ref_voltage_ = 0.0f;
        battery_percentage_ = 0;
        last_battery_check_time_ = 0;
        
        // 初始化电池提醒状态
        battery_remind_first_triggered_ = false;
        battery_remind_second_triggered_ = false;
        battery_remind_end_triggered_ = false;
        shutdown_in_progress_ = false;
        
        ESP_LOGI(TAG, "Battery monitor initialized on GPIO %d (ADC_CH%d)", 
                 BATTERY_ADC_GPIO, BATTERY_ADC_CHANNEL);
    }

    // 读取2.5V参考电压
    float ReadPowerReferenceVoltage()
    {
        int adc_reading = 0;
        int ref_mv = 0;
        
        for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle_, POWER_ADC_CHANNEL, &raw));
            adc_reading += raw;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        adc_reading /= BATTERY_ADC_SAMPLES;
        
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle_, adc_reading, &ref_mv));
        float ref_v = (float)ref_mv / 1000.0f;
        power_ref_voltage_ = ref_v;
        return ref_v;
    }

    // 读取电池电压
    float ReadBatteryVoltage()
    {
        int adc_reading = 0;
        int voltage_mv = 0;
        
        // 多次采样取平均值
        for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle_, BATTERY_ADC_CHANNEL, &raw));
            adc_reading += raw;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        adc_reading /= BATTERY_ADC_SAMPLES;
        
        
        // 转换为电压值
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle_, adc_reading, &voltage_mv));
        float voltage = (float)voltage_mv / 1000.0f;  // 转换为伏特（未校正）
        
        // 使用2.5V参考进行动态校正
        float ref_v = ReadPowerReferenceVoltage();
        float scale = 1.0f;
        if (ref_v > 0.001f) {
            scale = POWER_REF_VOLTAGE / ref_v;
        }
        float corrected_voltage = voltage * scale;
        
        // 考虑分压器比例
        float battery_voltage = corrected_voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;
        
        return battery_voltage;
    }

    // 计算电池电量百分比（根据实际电池特性分段计算）
    // 参考表格：
    // 100%----4.20V
    // 90%-----4.06V
    // 80%-----3.98V
    // 70%-----3.92V   
    // 60%-----3.87V   
    // 50%-----3.82V   
    // 40%-----3.79V   
    // 30%-----3.77V   
    // 20%-----3.74V   
    // 10%-----3.68V   
    // 5%------3.45V   
    // 0%------3.00V
    int CalculateBatteryPercentage(float voltage)
    {
        // Define the voltage to percentage mapping table
        struct VoltagePoint {
            float voltage;
            int percentage;
        };

        static const VoltagePoint kVoltageTable[] = {
            {4.15f, 100},
            {4.06f, 90},
            {3.98f, 80},
            {3.92f, 70},
            {3.87f, 60},
            {3.82f, 50},
            {3.75f, 40},
            {3.70f, 30},
            {3.60f, 20},
            {3.50f, 10},
            {3.40f, 5},
            {3.30f, 0}
        };
        
        const int kTableSize = sizeof(kVoltageTable) / sizeof(kVoltageTable[0]);

        // Voltage higher than max is 100%
        if (voltage >= kVoltageTable[0].voltage) {
            return 100;
        }

        // Voltage lower than min is 0%
        if (voltage <= kVoltageTable[kTableSize - 1].voltage) {
            return 0;
        }

        // Linear interpolation
        for (int i = 0; i < kTableSize - 1; ++i) {
            if (voltage >= kVoltageTable[i + 1].voltage) {
                float v_high = kVoltageTable[i].voltage;
                float v_low = kVoltageTable[i + 1].voltage;
                int p_high = kVoltageTable[i].percentage;
                int p_low = kVoltageTable[i + 1].percentage;

                float percentage = p_low + (voltage - v_low) * (p_high - p_low) / (v_high - v_low);
                return static_cast<int>(percentage);
            }
        }

        return 0;
    }

    // 关机函数
    void ShutdownDevice()
    {
        if (shutdown_in_progress_) {
            return; // 防止重复调用
        }
        shutdown_in_progress_ = true;
        
        ESP_LOGI(TAG, "Battery voltage too low (%.2fV), shutting down...", battery_voltage_);
        
        // 关闭音频输出
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        audio_service.Stop();
        
        // 关闭协议连接
        if (app.GetDeviceState() != kDeviceStateUnknown) {
            app.SetDeviceState(kDeviceStateIdle);
        }
        
        // 等待一小段时间确保音频播放完成
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ESP_LOGI(TAG, "Entering deep sleep (shutdown)");
        
        // 关闭电源控制（如果存在）
        // 这里可以根据实际硬件添加关闭外设的代码
        
        // 进入深度睡眠（关机）
        esp_deep_sleep_start();
    }

    // 更新电池状态
    void UpdateBatteryStatus()
    {
        int64_t current_time = esp_timer_get_time();
        
        // 每5秒检查一次电池状态
        if (current_time - last_battery_check_time_ >= 5000000) {  // 5秒 = 5000000微秒
            battery_voltage_ = ReadBatteryVoltage();
            battery_percentage_ = CalculateBatteryPercentage(battery_voltage_);
            last_battery_check_time_ = current_time;
            
            ESP_LOGI(TAG, "Battery: %.2fV (%d%%)", battery_voltage_, battery_percentage_);

            // 电池电量提醒逻辑
            auto& app = Application::GetInstance();
            
            // 低于5% - 最后一次提醒并关机
            if ( battery_percentage_ < 5 && !battery_remind_end_triggered_) {
                battery_remind_end_triggered_ = true;
                ESP_LOGW(TAG, "Battery critical: %.2fV, playing shutdown reminder", battery_voltage_);
                
                // 播放关机提醒音频
                // 注意：需要将 low_battery_off.ogg 放到 main/assets/common/ 目录
                extern const char _binary_low_battery_off_ogg_start[] asm("_binary_low_battery_off_ogg_start");
                extern const char _binary_low_battery_off_ogg_end[] asm("_binary_low_battery_off_ogg_end");
                std::string_view low_battery_off_sound(_binary_low_battery_off_ogg_start, 
                                                       _binary_low_battery_off_ogg_end - _binary_low_battery_off_ogg_start);
                // 切换esp32s3 音频输出通道
                gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_ENABLE_LEVEL);
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYOFF);
                vTaskDelay(pdMS_TO_TICKS(2000));
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_DISABLE_LEVEL);
                // 5秒后关机
                xTaskCreate([](void* arg) {
                    auto* self = static_cast<ai_martube_esp32s3*>(arg);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    self->ShutdownDevice();
                    vTaskDelete(NULL);
                }, "battery_shutdown", 4096, this, 5, nullptr);
            }
            // 等于5% - 第二次提醒
            else if (battery_percentage_ <= 5 && !battery_remind_second_triggered_) {
                battery_remind_second_triggered_ = true;
                ESP_LOGW(TAG, "Battery low: %.2fV, playing low battery reminder", battery_voltage_);
                
                // 播放低电量提醒音频
                // 注意：需要将 low_battery_remind.ogg 放到 main/assets/common/ 目录
                extern const char _binary_low_battery_remind_ogg_start[] asm("_binary_low_battery_remind_ogg_start");
                extern const char _binary_low_battery_remind_ogg_end[] asm("_binary_low_battery_remind_ogg_end");
                std::string_view low_battery_remind_sound(_binary_low_battery_remind_ogg_start, 
                                                          _binary_low_battery_remind_ogg_end - _binary_low_battery_remind_ogg_start);
                // 切换esp32s3 音频输出通道
                gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_ENABLE_LEVEL);
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYREMIND);
                vTaskDelay(pdMS_TO_TICKS(2000));
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_DISABLE_LEVEL);

            }
            // 低于10% - 第一次提醒
            else if (battery_percentage_ <= 10 && !battery_remind_first_triggered_) {
                battery_remind_first_triggered_ = true;
                ESP_LOGW(TAG, "Battery warning: %.2fV, playing low battery reminder", battery_voltage_);
                
                // 播放低电量提醒音频
                // 注意：需要将 low_battery_remind.ogg 放到 main/assets/common/ 目录
                extern const char _binary_low_battery_remind_ogg_start[] asm("_binary_low_battery_remind_ogg_start");
                extern const char _binary_low_battery_remind_ogg_end[] asm("_binary_low_battery_remind_ogg_end");
                std::string_view low_battery_remind_sound(_binary_low_battery_remind_ogg_start, 
                                                          _binary_low_battery_remind_ogg_end - _binary_low_battery_remind_ogg_start);

                // 切换esp32s3 音频输出通道
                gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_ENABLE_LEVEL);
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYREMIND);
                vTaskDelay(pdMS_TO_TICKS(2000));
                gpio_set_level(AUDIO_CODEC_PA_GPIO, AUDIO_CODEC_PA_GPIO_DISABLE_LEVEL);

            }
            
            // 如果电压回升，重置提醒状态（可选，根据需求决定）
            if (battery_percentage_ > 20) {
                battery_remind_first_triggered_ = false;
            }
            if (battery_percentage_ > 10) {
                battery_remind_second_triggered_ = false;
                battery_remind_end_triggered_ = false;
            }
        }
    }

    void InitializeTools()
    {
        auto &mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.light.set_brightness",
                           "设置桌面灯的亮度 (0-100)",
                           PropertyList({Property("brightness", kPropertyTypeInteger, 0, 100)}),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   int brightness = properties["brightness"].value<int>();
                                   pwm_led_controller_->SetBrightnessPercent(static_cast<uint8_t>(brightness));
                                   return true;
                               }
                               return false;
                           });

        mcp_server.AddTool("self.light.turn_on",
                           "打开桌面灯",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   pwm_led_controller_->TurnOn();
                                   return true;
                               }
                               return false;
                           });

        mcp_server.AddTool("self.light.turn_off",
                           "关闭桌面灯",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   pwm_led_controller_->TurnOff();
                                   return true;
                               }
                               return false;
                           });

        // 添加电机控制工具
        mcp_server.AddTool("self.motor.turn_on",
                           "打开电机",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_ENABLE_LEVEL);
                               ESP_LOGI(TAG, "Motor turned on");
                               return true;
                           });

        mcp_server.AddTool("self.motor.turn_off",
                           "关闭电机",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_DISABLE_LEVEL);
                               ESP_LOGI(TAG, "Motor turned off");
                               return true;
                           });

        mcp_server.AddTool("self.motor.get_status",
                           "获取电机状态",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               int level = gpio_get_level(MOTOR_CONTROL_GPIO);
                               return (level == MOTOR_CONTROL_ENABLE_LEVEL) ? "运行中" : "已关闭";
                           });

        // 添加电源控制工具
        mcp_server.AddTool("self.power.turn_on",
                           "开机",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               SetPowerState(true);
                               return true;
                           });

        mcp_server.AddTool("self.power.turn_off",
                           "关机",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               SetPowerState(false);
                               return true;
                           });

    }

public:
    ai_martube_esp32s3() :  boot_button_(BOOT_BUTTON_GPIO), 
                            shutdown_button_(SHUTDOWN_BUTTON_GPIO, true, 3000, 0, false), 
                            key_input_button_(KEY_INPUT_GPIO, false, 3000, 0, false),
                            pwm_led_(nullptr), 
                            pwm_led_controller_(nullptr)
    {
        InitializePowerControl();
        InitializeAudioSwitch();
        InitializeMotor();
        InitializeChargeStatus();
        InitializeBatteryMonitor();
        InitializeEncoder();
        InitializeCustomInput();
        InitializeI2c();
        InitializeButtons();
        // 先确保板级 LED 已创建，再实例化控制器
        if (!pwm_led_)
        {
            // 使用正确的GpioLed构造函数参数
            pwm_led_ = new GpioLed(PWM_LED_GPIO, PWM_LED_OUTPUT_INVERT, PWM_LED_TIMER, PWM_LED_CHANNEL);
        }
        //启动呼吸效果
        pwm_led_controller_ = new PwmLedController(pwm_led_);
        {
            breath_min_ = 5;
            breath_max_ = 100;
            breath_step_ = 2;
            breath_interval_ms_ = 30;
            breath_current_ = breath_min_;
            pwm_led_->SetBrightness(breath_current_);
            pwm_led_->TurnOn();
            esp_timer_create_args_t args = {};
            args.callback = &BootBreathTimerCallback;
            args.arg = this;
            args.dispatch_method = ESP_TIMER_TASK;
            args.name = "boot_breath";
            args.skip_unhandled_events = false;
            ESP_ERROR_CHECK(esp_timer_create(&args, &boot_breath_timer_));
            boot_breathing_ = true;
            esp_timer_start_periodic(boot_breath_timer_, breath_interval_ms_ * 1000);
        }
    
        
        
        InitializeTools();

        // 新增：初始化串口通信模块，并设置数据解析回调
        uart_comm_ = new UartComm(BOARD_UART_PORT, BOARD_UART_TX, BOARD_UART_RX, BOARD_UART_BAUD);
        uart_comm_->Begin();
        uart_comm_->SetParser([this](const uint8_t *data, size_t len)
        {
            // 按长度输出十六进制字符串，避免把二进制当作 %s 打印
            std::string hex;
            hex.reserve(len * 3);
            for (size_t i = 0; i < len; ++i) {
                char b[4];
                snprintf(b, sizeof(b), "%02X ", data[i]);
                hex += b;
            }
            ESP_LOGI(TAG, "UART received %d bytes: %s", (int)len, hex.c_str());
            // 如果收到16进制数据：AA 00 01 01
            if (len == 4 && data[0] == 0xAA && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x01) {
                
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateIdle && !bluetooth_mode_) {
                    gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                    gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);

                    
                    std::string wake_word = "你好小王子";
                    ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                    app.WakeWordInvoke(wake_word);
                }
            } 
        });
        // 新增：设备状态监控线程，按状态启停音频控制IO，同时监控开关输入
        xTaskCreatePinnedToCore(
            [](void* arg) {
                auto* self = static_cast<ai_martube_esp32s3*>(arg);
                auto& app = Application::GetInstance();
                DeviceState last = app.GetDeviceState();
                int last_custom_input_level = gpio_get_level(CUSTOM_INPUT_GPIO);
                bool first_connect_reminder = true;

                Settings settings("audio", false);
                self->current_volume_ = settings.GetInt("output_volume", 70);
                
                while (true) {
                    // 音频控制逻辑
                    DeviceState cur = app.GetDeviceState();
                    
                    // 根据设备状态切换音频模式
                    if (cur != last) {
                        if (cur == kDeviceStateIdle) {
                            // 连接成功后关闭呼吸灯
                            if (first_connect_reminder) {
                                if (self->boot_breath_timer_) {
                                    self->boot_breathing_ = false;
                                    esp_timer_stop(self->boot_breath_timer_);
                                }
                                if (self->pwm_led_) {
                                    self->pwm_led_->TurnOff();
                                }
                                first_connect_reminder = false;
                            }

                            // 进入待机状态时，切换到蓝牙模式
                            gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                            ESP_LOGI(TAG, "Device state changed to idle, switching to Bluetooth audio mode");
                        }
                        
                        last = cur;

                    }
                    // 开关输入检测逻辑
                    int current_custom_input_level = gpio_get_level(CUSTOM_INPUT_GPIO);
                    if (current_custom_input_level != last_custom_input_level) {
                        ESP_LOGI(TAG, "Custom input state changed: %d -> %d", last_custom_input_level, current_custom_input_level);
                        
                        // 根据开关状态控制电机
                        if (current_custom_input_level == 0) {
                            // 开关闭合（低电平）
                            self->OnCustomInputLowEvent();
                            ESP_LOGI(TAG, "Motor enabled by custom input");
                        } else {
                            // 开关断开（高电平）
                            self->OnCustomInputHighEvent();
                            ESP_LOGI(TAG, "Motor disabled by custom input");
                        }
                        
                        last_custom_input_level = current_custom_input_level;
                    }

                    
                    // 更新电池状态
                    self->UpdateBatteryStatus();

                    self->UpdateChargeStatus();
                    
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            },
            "dev_state_monitor",
            4096,
            this,
            5,
            nullptr,
            0
        );

        // 网络状态监测线程：周期性检测 Wi-Fi 连接状态并打印日志
        // xTaskCreatePinnedToCore(
        //     [](void* /*arg*/) {
        //         bool last_connected = WifiStation::GetInstance().IsConnected();
        //         ESP_LOGI(TAG, "Network monitor start, connected=%d", last_connected);
        //         while (true) {
        //             bool connected = WifiStation::GetInstance().IsConnected();
        //             // 读取当前 RSSI（仅在已连接时有效）
        //             if (connected) {
        //                 int8_t rssi = WifiStation::GetInstance().GetRssi();
        //                 ESP_LOGI(TAG, "WiFi RSSI: %d dBm", rssi);
        //             }
        //             if (connected != last_connected) {
        //                 ESP_LOGI(TAG, "Network state changed: %s",
        //                          connected ? "connected" : "disconnected");
        //                 last_connected = connected;
        //             }
        //             vTaskDelay(pdMS_TO_TICKS(2000)); // 2s 检测一次
        //         }
        //     },
        //     "net_monitor",
        //     2048,
        //     nullptr,
        //     4,
        //     nullptr,
        //     0
        // );
    }



    virtual AudioCodec *GetAudioCodec() override
    {

        static Es8389AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_GPIO,
            AUDIO_CODEC_ES8389_ADDR,
            true);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        // return display_;
        static NoDisplay display;
        return &display;
    }

    virtual Camera *GetCamera() override
    {
        // return camera_;
        return nullptr;
    }

    virtual PwmLedController *GetPwmLedController() override
    {
        return pwm_led_controller_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override
    {
        level = battery_percentage_;
        charging = false;
        discharging = false;
        return true;
    }
    
};

DECLARE_BOARD(ai_martube_esp32s3);
