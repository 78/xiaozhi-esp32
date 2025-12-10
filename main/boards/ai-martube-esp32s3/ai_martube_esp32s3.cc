#include "wifi_board.h"
#include "codecs/es8388_audio_codec.h"
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

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
    LcdDisplay *display_;
    XL9555 *xl9555_;
    Esp32Camera *camera_;
    GpioLed *pwm_led_;                     // 添加PWM LED成员变量
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
    static QueueHandle_t key_event_queue_;
    int64_t key_press_time_;
    bool key_is_pressed_;
    uint8_t light_mode_ = 2;  // 灯光模式：0=灭, 1=30%亮度, 2=100%亮度
    std::function<void()> on_short_press_callback_;  // 短按事件回调
    std::function<void()> on_long_press_callback_;   // 长按事件回调
    bool key_long_press_triggered_;

    // 开关机按键相关变量
    static QueueHandle_t shutdown_event_queue_;
    int64_t shutdown_press_time_;
    bool shutdown_is_pressed_;
    std::function<void()> on_shutdown_long_press_callback_;  // 关机长按事件回调
    bool shutdown_long_press_triggered_;

    // 自定义输入信号相关变量
    int custom_input_last_level_;
    bool custom_input_event_triggered_;  // 标记是否已触发，确保只触发一次
    int64_t custom_input_last_change_time_;
    std::function<void()> on_custom_input_high_callback_;  // 高电平事件回调
    std::function<void()> on_custom_input_low_callback_;   // 低电平事件回调

    // 旋转编码器相关变量
    int encoder_a_last_state_;
    int encoder_b_last_state_;
    int current_volume_;  // 当前音量 (0-100)
    std::function<void(int)> on_volume_change_callback_;  // 音量变化回调
    int64_t last_a_change_time_us_ = 0;
    int64_t last_b_change_time_us_ = 0;
    int last_ab_state_ = 0;
    int prev_read_ab_ = 0;
    int stable_count_ = 0;
    int pulse_accum_ = 0;
    int64_t window_start_us_ = 0;
    int64_t last_ab_change_time_us_ = 0;
    
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
            app.ToggleChatState(); });
    }

    void InitializeSt7789Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        // 液晶屏控制IO初始化
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS_PIN;
        io_config.dc_gpio_num = LCD_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 7;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io);

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel);

        esp_lcd_panel_reset(panel);
        // xl9555_->SetOutputState(8, 1);
        // xl9555_->SetOutputState(2, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 初始化摄像头：ov2640；
    // 根据正点原子官方示例参数
    void InitializeCamera()
    {

        xl9555_->SetOutputState(OV_PWDN_IO, 0);  // PWDN=低 (上电)
        xl9555_->SetOutputState(OV_RESET_IO, 0); // 确保复位
        vTaskDelay(pdMS_TO_TICKS(50));           // 延长复位保持时间
        xl9555_->SetOutputState(OV_RESET_IO, 1); // 释放复位
        vTaskDelay(pdMS_TO_TICKS(50));           // 延长 50ms

        camera_config_t config = {};

        config.pin_pwdn = CAM_PIN_PWDN;   // 实际由 XL9555 控制
        config.pin_reset = CAM_PIN_RESET; // 实际由 XL9555 控制
        config.pin_xclk = CAM_PIN_XCLK;
        config.pin_sccb_sda = CAM_PIN_SIOD;
        config.pin_sccb_scl = CAM_PIN_SIOC;

        config.pin_d7 = CAM_PIN_D7;
        config.pin_d6 = CAM_PIN_D6;
        config.pin_d5 = CAM_PIN_D5;
        config.pin_d4 = CAM_PIN_D4;
        config.pin_d3 = CAM_PIN_D3;
        config.pin_d2 = CAM_PIN_D2;
        config.pin_d1 = CAM_PIN_D1;
        config.pin_d0 = CAM_PIN_D0;
        config.pin_vsync = CAM_PIN_VSYNC;
        config.pin_href = CAM_PIN_HREF;
        config.pin_pclk = CAM_PIN_PCLK;

        /* XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental) */
        config.xclk_freq_hz = 24000000;
        config.ledc_timer = LEDC_TIMER_0;
        config.ledc_channel = LEDC_CHANNEL_0;

        config.pixel_format = PIXFORMAT_RGB565; /* YUV422,GRAYSCALE,RGB565,JPEG */
        config.frame_size = FRAMESIZE_QVGA;     /* QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates */

        config.jpeg_quality = 12; /* 0-63, for OV series camera sensors, lower number means higher quality */
        config.fb_count = 2;      /* When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode */
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        esp_err_t err = esp_camera_init(&config); // 测试相机是否存在
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Camera is not plugged in or not supported, error: %s", esp_err_to_name(err));
            // 如果摄像头初始化失败，设置 camera_ 为 nullptr
            camera_ = nullptr;
            return;
        }
        else
        {
            esp_camera_deinit(); // 释放之前的摄像头资源,为正确初始化做准备
            camera_ = new Esp32Camera(config);
        }
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

    // 初始化开关输入IO：配置为输入模式，禁用内部上下拉
    void InitializeSwitchInput()
    {
        gpio_config_t switch_init_struct = {0};

        switch_init_struct.mode = GPIO_MODE_INPUT;
        switch_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        switch_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        switch_init_struct.pin_bit_mask = 1ull << SWITCH_INPUT_GPIO;
        ESP_ERROR_CHECK(gpio_config(&switch_init_struct));
        
        // 读取当前电平
        int initial_level = gpio_get_level(SWITCH_INPUT_GPIO);
        ESP_LOGI(TAG, "Switch input initialized on GPIO %d, initial status: %d", SWITCH_INPUT_GPIO, initial_level);
        
        // 延迟后再次读取，确认是否稳定
        vTaskDelay(pdMS_TO_TICKS(100));
        int level_after_delay = gpio_get_level(SWITCH_INPUT_GPIO);
        ESP_LOGI(TAG, "Switch input status after 100ms: %d", level_after_delay);
    }

    // 初始化按键输入IO：配置为输入模式，启用上拉
    void InitializeKeyInput()
    {
        gpio_config_t key_init_struct = {0};

        key_init_struct.mode = GPIO_MODE_INPUT;
        key_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        key_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        key_init_struct.pin_bit_mask = 1ull << KEY_INPUT_GPIO;
        ESP_ERROR_CHECK(gpio_config(&key_init_struct));
        
        // 创建按键事件队列
        key_event_queue_ = xQueueCreate(10, sizeof(KeyEvent));
        if (key_event_queue_ == NULL) {
            ESP_LOGE(TAG, "Failed to create key event queue");
        }
        
        // 初始化按键状态
        key_press_time_ = 0;
        key_is_pressed_ = false;
        key_long_press_triggered_ = false;
        light_mode_ = 0;  // 初始为关闭状态
        
        // 读取当前电平
        int initial_level = gpio_get_level(KEY_INPUT_GPIO);
        ESP_LOGI(TAG, "Key input initialized on GPIO %d, initial status: %d", KEY_INPUT_GPIO, initial_level);
    }

    // 设置按键事件回调
    void SetKeyCallbacks(std::function<void()> short_press_cb, std::function<void()> long_press_cb)
    {
        on_short_press_callback_ = short_press_cb;
        on_long_press_callback_ = long_press_cb;
    }

    // 处理按键事件
    void ProcessKeyEvent()
    {
        KeyEvent event;
        while (xQueueReceive(key_event_queue_, &event, 0) == pdTRUE) {
            int64_t current_time = esp_timer_get_time();
            
            if (event.type == KEY_EVENT_PRESS) {
                if (!key_is_pressed_) {
                    key_is_pressed_ = true;
                    key_press_time_ = current_time;
                    key_long_press_triggered_ = false;
                    ESP_LOGI(TAG, "Key pressed at %lld", key_press_time_);
                }
            } else if (event.type == KEY_EVENT_RELEASE) {
                if (key_is_pressed_) {
                    key_is_pressed_ = false;
                    int64_t press_duration = current_time - key_press_time_;
                    ESP_LOGI(TAG, "Key released, duration: %lld us", press_duration);
                    
                    if (!key_long_press_triggered_ && press_duration >= KEY_DEBOUNCE_TIME_MS * 1000) {
                        // 短按
                        ESP_LOGI(TAG, "Short press detected");
                        if (on_short_press_callback_) {
                            on_short_press_callback_();
                        }
                    }
                    key_long_press_triggered_ = false;
                }
            }
        }

        // 即时长按：无需等待释放，超过阈值立即触发一次
        if (key_is_pressed_ && !key_long_press_triggered_) {
            int64_t now = esp_timer_get_time();
            int64_t duration = now - key_press_time_;
            if (duration >= KEY_LONG_PRESS_TIME_MS * 1000) {
                key_long_press_triggered_ = true;
                ESP_LOGI(TAG, "Long press detected (instant)");
                if (on_long_press_callback_) {
                    on_long_press_callback_();
                }
            }
        }
    }

    // 控制灯光模式
    void SetLightMode(uint8_t mode)
    {
        // auto& app = Application::GetInstance(); 
        // gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
        // EnablePowerAmplifier();
        // app.PlaySound(Lang::Sounds::OGG_WIFICONFIG);
        // vTaskDelay(pdMS_TO_TICKS(2000));
        // DisablePowerAmplifier();
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

    // 短按处理：循环切换灯光模式，在上电时则进入配网模式
    void OnShortPress()
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

    // 长按处理
    void OnLongPress()
    {
        auto& app = Application::GetInstance();
        // 长切换模式：AI 模式 <-> 蓝牙模式，并发送串口命令
        // AI -> 蓝牙：A5 00 02 05
        // 蓝牙 -> AI：A5 00 02 06
        if (!bluetooth_mode_) {
            // 提醒用户切换到蓝牙模式
            gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
            EnablePowerAmplifier();
            app.PlaySound(Lang::Sounds::OGG_BTMODE);
            vTaskDelay(pdMS_TO_TICKS(2000));
            DisablePowerAmplifier();

            // 设备状态设置为待机
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() != kDeviceStateIdle)  {
                app.SetDeviceState(kDeviceStateIdle);
            }
            
            // 切到蓝牙模式
            gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
            gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
            DisablePowerAmplifier();
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
            EnablePowerAmplifier();
            app.PlaySound(Lang::Sounds::OGG_AIMODE);
            vTaskDelay(pdMS_TO_TICKS(2000));
            DisablePowerAmplifier();

            // 切到 AI 模式
            gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
            gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
            EnablePowerAmplifier();
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
            app.InvokeWakeWord(wake_word);
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
        // vTaskDelay(pdMS_TO_TICKS(2000));
        // if (gpio_get_level(SHUTDOWN_BUTTON_GPIO) == 0) {
            gpio_set_level(POWER_ON_CONTROL_GPIO, 1);
        // }
        // else{
        //     gpio_set_level(POWER_ON_CONTROL_GPIO, 0);
        // }
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
        
        // 初始化状态
        custom_input_last_level_ = gpio_get_level(CUSTOM_INPUT_GPIO);
        custom_input_event_triggered_ = false;
        custom_input_last_change_time_ = esp_timer_get_time();
        
        ESP_LOGI(TAG, "Custom input initialized on GPIO %d, initial status: %d", CUSTOM_INPUT_GPIO, custom_input_last_level_);
    }

    // 设置自定义输入事件回调
    void SetCustomInputCallbacks(std::function<void()> high_cb, std::function<void()> low_cb)
    {
        on_custom_input_high_callback_ = high_cb;
        on_custom_input_low_callback_ = low_cb;
    }

    // 处理自定义输入变化（只触发一次）
    void ProcessCustomInput()
    {
        int current_level = gpio_get_level(CUSTOM_INPUT_GPIO);
        int64_t now = esp_timer_get_time();
        
        // 检测电平变化
        if (current_level != custom_input_last_level_) {
            // 检查去抖时间
            if (now - custom_input_last_change_time_ >= CUSTOM_INPUT_DEBOUNCE_TIME_MS * 1000) {
                // 确保只触发一次
                if (!custom_input_event_triggered_) {
                    ESP_LOGI(TAG, "Custom input level changed: %d -> %d", custom_input_last_level_, current_level);
                    
                    // 触发对应的事件回调
                    if (current_level == 1 && on_custom_input_high_callback_) {
                        // 高电平触发
                        ESP_LOGI(TAG, "Custom input HIGH level event triggered");
                        on_custom_input_high_callback_();
                    } else if (current_level == 0 && on_custom_input_low_callback_) {
                        // 低电平触发
                        ESP_LOGI(TAG, "Custom input LOW level event triggered");
                        on_custom_input_low_callback_();
                    }
                    
                    // 标记已触发，确保只触发一次
                    custom_input_event_triggered_ = true;
                    custom_input_last_level_ = current_level;
                }
            }
            custom_input_last_change_time_ = now;
        } else {
            // 电平稳定后，重置触发标志，允许下次变化再次触发
            if (now - custom_input_last_change_time_ >= CUSTOM_INPUT_DEBOUNCE_TIME_MS * 1000) {
                custom_input_event_triggered_ = false;
            }
        }
    }

    // 初始化开关机按键输入IO：配置为输入模式，启用上拉
    void InitializeShutdownButton()
    {
        gpio_config_t shutdown_init_struct = {0};

        shutdown_init_struct.mode = GPIO_MODE_INPUT;
        shutdown_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        shutdown_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        shutdown_init_struct.pin_bit_mask = 1ull << SHUTDOWN_BUTTON_GPIO;
        ESP_ERROR_CHECK(gpio_config(&shutdown_init_struct));
        
        // 创建开关机按键事件队列
        shutdown_event_queue_ = xQueueCreate(5, sizeof(KeyEvent));
        if (shutdown_event_queue_ == NULL) {
            ESP_LOGE(TAG, "Failed to create shutdown event queue");
        }
        
        // 初始化开关机按键状态
        shutdown_press_time_ = 0;
        shutdown_is_pressed_ = false;
        
        // 读取当前电平
        int initial_level = gpio_get_level(SHUTDOWN_BUTTON_GPIO);
        ESP_LOGI(TAG, "Shutdown button initialized on GPIO %d, initial status: %d", SHUTDOWN_BUTTON_GPIO, initial_level);
        shutdown_long_press_triggered_ = false;
    }

    // 设置开关机按键事件回调
    void SetShutdownCallback(std::function<void()> long_press_cb)
    {
        on_shutdown_long_press_callback_ = long_press_cb;
    }

    // 处理开关机按键事件
    void ProcessShutdownEvent()
    {
        KeyEvent event;
        while (xQueueReceive(shutdown_event_queue_, &event, 0) == pdTRUE) {
            int64_t current_time = esp_timer_get_time();
            
            if (event.type == KEY_EVENT_PRESS) {
                if (!shutdown_is_pressed_) {
                    shutdown_is_pressed_ = true;
                    shutdown_press_time_ = current_time;
                    shutdown_long_press_triggered_ = false;
                    ESP_LOGI(TAG, "Shutdown button pressed at %lld", shutdown_press_time_);
                }
            } else if (event.type == KEY_EVENT_RELEASE) {
                if (shutdown_is_pressed_) {
                    shutdown_is_pressed_ = false;
                    int64_t press_duration = current_time - shutdown_press_time_;
                    ESP_LOGI(TAG, "Shutdown button released, duration: %lld us", press_duration);
                    
                    if (!shutdown_long_press_triggered_) {
                        if (press_duration >= SHUTDOWN_LONG_PRESS_TIME_MS * 1000) {
                            ESP_LOGI(TAG, "Shutdown long press detected");
                            if (on_shutdown_long_press_callback_) {
                                on_shutdown_long_press_callback_();
                            }
                        } else if (press_duration >= SHUTDOWN_DEBOUNCE_TIME_MS * 1000) {
                        auto& app = Application::GetInstance();
                        if (app.GetDeviceState() == kDeviceStateSpeaking) {
                            ESP_LOGI(TAG, "Shutdown short press: abort speaking and enter listening");
                            app.ToggleChatState();
                        } else {
                            ESP_LOGI(TAG, "Shutdown short press ignored: current state not speaking");
                        }
                    }
                    }
                    shutdown_long_press_triggered_ = false;
                    
                    
                }
            }
        }
        if (shutdown_is_pressed_ && !shutdown_long_press_triggered_) {
            int64_t now = esp_timer_get_time();
            if (now - shutdown_press_time_ >= SHUTDOWN_LONG_PRESS_TIME_MS * 1000) {
                shutdown_long_press_triggered_ = true;
                ESP_LOGI(TAG, "Shutdown long press detected (instant)");
                if (on_shutdown_long_press_callback_) {
                    on_shutdown_long_press_callback_();
                }
            }
        }
    }

    // 关机长按处理
    void OnShutdownLongPress()
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
        EnablePowerAmplifier();
        app.PlaySound(Lang::Sounds::OGG_POWEROFF);
        vTaskDelay(pdMS_TO_TICKS(1000));
        DisablePowerAmplifier();
        vTaskDelay(pdMS_TO_TICKS(100));
        SetPowerState(false);
        

    }

    // 电源控制函数
    void SetPowerState(bool power_on)
    {
        gpio_set_level(POWER_ON_CONTROL_GPIO, power_on ? 1 : 0);
        ESP_LOGI(TAG, "Power control set to %s", power_on ? "HIGH (ON)" : "LOW (OFF)");
    }

    // 功放使能函数
    void EnablePowerAmplifier()
    {
        gpio_set_level(AUDIO_CODEC_PA_GPIO, 0);
        ESP_LOGI(TAG, "Power amplifier enabled on GPIO %d", AUDIO_CODEC_PA_GPIO);
    }

    // 功放禁用函数
    void DisablePowerAmplifier()
    {
        gpio_set_level(AUDIO_CODEC_PA_GPIO, 1);
        ESP_LOGI(TAG, "Power amplifier disabled on GPIO %d", AUDIO_CODEC_PA_GPIO);
    }

    // 初始化旋转编码器：配置为输入模式，启用上拉
    void InitializeEncoder()
    {
        gpio_config_t encoder_a_config = {0};
        encoder_a_config.mode = GPIO_MODE_INPUT;
        encoder_a_config.pull_up_en = GPIO_PULLUP_ENABLE;
        encoder_a_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        encoder_a_config.pin_bit_mask = 1ull << ENCODER_A_GPIO;
        ESP_ERROR_CHECK(gpio_config(&encoder_a_config));

        gpio_config_t encoder_b_config = {0};
        encoder_b_config.mode = GPIO_MODE_INPUT;
        encoder_b_config.pull_up_en = GPIO_PULLUP_ENABLE;
        encoder_b_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        encoder_b_config.pin_bit_mask = 1ull << ENCODER_B_GPIO;
        ESP_ERROR_CHECK(gpio_config(&encoder_b_config));

        // 初始化编码器状态
        encoder_a_last_state_ = gpio_get_level(ENCODER_A_GPIO);
        encoder_b_last_state_ = gpio_get_level(ENCODER_B_GPIO);
        current_volume_ = 50;  // 初始音量设为50%

        last_ab_state_ = (encoder_a_last_state_ << 1) | encoder_b_last_state_;
        prev_read_ab_ = last_ab_state_;
        stable_count_ = 0;
        pulse_accum_ = 0;
        window_start_us_ = 0;
        last_ab_change_time_us_ = 0;

        ESP_LOGI(TAG, "Encoder initialized - A: GPIO%d=%d, B: GPIO%d=%d, Volume: %d%%", 
                 ENCODER_A_GPIO, encoder_a_last_state_, 
                 ENCODER_B_GPIO, encoder_b_last_state_, 
                 current_volume_);
    }

    // 设置音量变化回调
    void SetVolumeChangeCallback(std::function<void(int)> callback)
    {
        on_volume_change_callback_ = callback;
    }

    // 处理旋转编码器
    void ProcessEncoder()
    {
        int a_now = gpio_get_level(ENCODER_A_GPIO);
        int b_now = gpio_get_level(ENCODER_B_GPIO);
        int ab_now = (a_now << 1) | b_now;
        int64_t now_us = esp_timer_get_time();
        auto& app = Application::GetInstance();

        if (ab_now != last_ab_state_) {
            if (now_us - last_ab_change_time_us_ >= ENCODER_DEBOUNCE_US) {
                int t = (last_ab_state_ << 2) | ab_now;
                bool cw = (t == ((0 << 2) | 2)) || (t == ((2 << 2) | 3)) || (t == ((3 << 2) | 1)) || (t == ((1 << 2) | 0));
                bool ccw = (t == ((0 << 2) | 1)) || (t == ((1 << 2) | 3)) || (t == ((3 << 2) | 2)) || (t == ((2 << 2) | 0));
                if (cw || ccw) {
                    if (pulse_accum_ == 0) {
                        window_start_us_ = now_us;
                    }
                    pulse_accum_ += cw ? 1 : -1;
                }
                last_ab_state_ = ab_now;
                last_ab_change_time_us_ = now_us;
            }
        }

        if (pulse_accum_ != 0) {
            if (now_us - window_start_us_ > ENCODER_WINDOW_US) {
                pulse_accum_ = 0;
                window_start_us_ = 0;
            }
        }

        if (pulse_accum_ >= 2) {
            if (bluetooth_mode_) {
                int steps = pulse_accum_ / 2;
                for (int i = 0; i < steps; ++i) {
                    uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x01};
                    if (uart_comm_ && uart_comm_->IsReady()) {
                        uart_comm_->Send(cmd, sizeof(cmd));
                    }
                }
            } else {
                int steps = pulse_accum_ / 2;
                int delta = steps * 5;
                int new_volume = current_volume_ + delta;
                if (new_volume > 100) new_volume = 100;
                //如果音量相同，不处理
                if (new_volume != current_volume_) {
                    // 播放提示音
                    gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                    EnablePowerAmplifier();
                    app.PlaySound(Lang::Sounds::OGG_SOUNDSET);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    DisablePowerAmplifier();
                    OnVolumeChange(new_volume);
                }
                else if (new_volume == 100)
                {
                    // 播放提示音
                    gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                    EnablePowerAmplifier();
                    app.PlaySound(Lang::Sounds::OGG_MAXSOUND);
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    DisablePowerAmplifier();
                    OnVolumeChange(new_volume);
                }
                
            }
            pulse_accum_ -= (pulse_accum_ / 2) * 2;
            if (pulse_accum_ == 0) {
                window_start_us_ = 0;
            }
        } else if (pulse_accum_ <= -2) {
            if (bluetooth_mode_) {
                int steps = (-pulse_accum_) / 2;
                for (int i = 0; i < steps; ++i) {
                    uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x02};
                    if (uart_comm_ && uart_comm_->IsReady()) {
                        uart_comm_->Send(cmd, sizeof(cmd));
                    }
                }
            } else {
                int steps = (-pulse_accum_) / 2;
                int delta = steps * 5;
                int new_volume = current_volume_ - delta;
                if (new_volume < 0) new_volume = 0;
                //如果音量相同，不处理
                if (new_volume != current_volume_) {
                    // 播放提示音
                    gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                    EnablePowerAmplifier();
                    app.PlaySound(Lang::Sounds::OGG_SOUNDSET);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    DisablePowerAmplifier();
                    OnVolumeChange(new_volume);
                }
            }
            pulse_accum_ += ((-pulse_accum_) / 2) * 2;
            if (pulse_accum_ == 0) {
                window_start_us_ = 0;
            }
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
                EnablePowerAmplifier();
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYOFF);
                vTaskDelay(pdMS_TO_TICKS(2000));
                DisablePowerAmplifier();
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
                EnablePowerAmplifier();
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYREMIND);
                vTaskDelay(pdMS_TO_TICKS(2000));
                DisablePowerAmplifier();

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
                EnablePowerAmplifier();
                vTaskDelay(pdMS_TO_TICKS(50));
                app.PlaySound(Lang::Sounds::OGG_BATTERYREMIND);
                vTaskDelay(pdMS_TO_TICKS(2000));
                DisablePowerAmplifier();

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

        // 添加桌面灯控制工具
        mcp_server.AddTool("self.light.get_brightness",
                           "获取桌面灯的亮度状态",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   return static_cast<int>(pwm_led_controller_->LastBrightnessPercent());
                               }
                               return "灯状态查询功能暂不可用";
                           });

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

        mcp_server.AddTool("self.light.blink_once",
                           "桌面灯闪烁一次",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   pwm_led_controller_->BlinkOnce();
                                   return true;
                               }
                               return false;
                           });

        mcp_server.AddTool("self.light.start_continuous_blink",
                           "桌面灯持续闪烁",
                           PropertyList({Property("interval", kPropertyTypeInteger, 100, 5000)}),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               if (pwm_led_controller_ && pwm_led_controller_->IsReady())
                               {
                                   int interval = properties["interval"].value<int>();
                                   pwm_led_controller_->StartContinuousBlink(interval);
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

        mcp_server.AddTool("self.power.get_status",
                           "获取电源状态",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               int level = gpio_get_level(POWER_ON_CONTROL_GPIO);
                               return (level == 1) ? "开机" : "关机";
                           });

        // 添加获取当前音量的工具（全局只有设置工具）
        mcp_server.AddTool("self.volume.get",
                           "获取当前音量",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               return current_volume_;
                           });

        // 添加电池电量工具
        mcp_server.AddTool("self.battery.get_voltage",
                           "获取电池电压",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               UpdateBatteryStatus();
                               return std::to_string(battery_voltage_);
                           });

        mcp_server.AddTool("self.battery.get_percentage",
                           "获取电池电量百分比",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               UpdateBatteryStatus();
                               return battery_percentage_;
                           });

        mcp_server.AddTool("self.battery.get_status",
                           "获取电池状态信息",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               UpdateBatteryStatus();
                               std::string status = "电压: " + std::to_string(battery_voltage_) + 
                                                   "V, 电量: " + std::to_string(battery_percentage_) + "%";
                               return status;
                           });
    }

public:
    ai_martube_esp32s3() : boot_button_(BOOT_BUTTON_GPIO), pwm_led_(nullptr), pwm_led_controller_(nullptr)
    {
        InitializeShutdownButton();
        InitializePowerControl();
        InitializeAudioSwitch();
        InitializeMotor();
        InitializeChargeStatus();
        InitializeBatteryMonitor();
        // InitializeSwitchInput();
        InitializeKeyInput();
        InitializeEncoder();
        InitializeCustomInput();
        InitializeI2c();
        // InitializeSpi();
        // InitializeSt7789Display();
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
        // 设置按键回调
        SetKeyCallbacks(
            [this]() { OnShortPress(); },  // 短按回调
            [this]() { OnLongPress(); }    // 长按回调
        );
        
        // 设置开关机按键回调
        SetShutdownCallback(
            [this]() { OnShutdownLongPress(); }  // 关机长按回调
        );

        // 设置自定义输入回调
        SetCustomInputCallbacks(
            [this]() { OnCustomInputHighEvent(); },
            [this]() { OnCustomInputLowEvent(); }
        );
        
        // 设置音量变化回调
        SetVolumeChangeCallback(
            [this](int volume) { OnVolumeChange(volume); }  // 音量变化回调
        );
        
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
                    EnablePowerAmplifier();

                    
                    std::string wake_word = "你好小王子";
                    ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                    app.InvokeWakeWord(wake_word);
                }
            } 
        });
        // 新增：设备状态监控线程，按状态启停音频控制IO，同时监控开关输入
        xTaskCreatePinnedToCore(
            [](void* arg) {
                auto* self = static_cast<ai_martube_esp32s3*>(arg);
                auto& app = Application::GetInstance();
                DeviceState last = app.GetDeviceState();
                int last_switch_level = gpio_get_level(SWITCH_INPUT_GPIO);
                int last_key_level = gpio_get_level(KEY_INPUT_GPIO);
                int last_shutdown_level = gpio_get_level(SHUTDOWN_BUTTON_GPIO);
                // 上电后首次成功连接提醒标志位
                bool first_connect_reminder = true;
                
                while (true) {
                    // 音频控制逻辑
                    DeviceState cur = app.GetDeviceState();
                    
                    // 根据设备状态切换音频模式
                    if (cur != last) {
                        if (cur == kDeviceStateIdle) {
                            // 连接成功后首次提醒
                            if (first_connect_reminder) {
                                self->EnablePowerAmplifier();
                                vTaskDelay(pdMS_TO_TICKS(10));
                                // 在main\application.cc的542处有提醒，所以这里暂时不开启，延时的目的是等待那里播放后才禁用功放
                                // app.PlaySound(Lang::Sounds::OGG_SUCCESS);
                                vTaskDelay(pdMS_TO_TICKS(2000));
                                first_connect_reminder = false;
                            }
                            if (self->boot_breath_timer_) {
                                self->boot_breathing_ = false;
                                esp_timer_stop(self->boot_breath_timer_);
                            }
                            if (self->pwm_led_) {
                                self->pwm_led_->TurnOff();
                            }
                            // 进入待机状态时，切换到蓝牙模式
                            gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                            ESP_LOGI(TAG, "Device state changed to idle, switching to Bluetooth audio mode");
                            self->DisablePowerAmplifier();
                        }
                        else if (cur == kDeviceStateSpeaking) {
                            // 进入说话状态时，确保输出通道在 ESP32S3
                            self->EnablePowerAmplifier();
                            ESP_LOGI(TAG, "Device state changed to speaking, ensuring ESP32S3 output");
                        }
                        else if (cur == kDeviceStateListening) {
                            // 进入监听状态时，确保输入通道在 ESP32S3
                            gpio_set_level(SWITCH_INPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                            ESP_LOGI(TAG, "Device state changed to listening, ensuring ESP32S3 input");
                            // 监听时不需要功放
                            self->DisablePowerAmplifier();
                        }
                        else if (cur == kDeviceStateWifiConfiguring) {
                            gpio_set_level(SWITCH_OUTPUT_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                            self->EnablePowerAmplifier();
                        } else {
                            self->DisablePowerAmplifier();
                        }
                        
                        last = cur;

                    }
                    // 开关输入检测逻辑
                    int current_switch_level = gpio_get_level(SWITCH_INPUT_GPIO);
                    if (current_switch_level != last_switch_level) {
                        ESP_LOGI(TAG, "Switch state changed: %d -> %d", last_switch_level, current_switch_level);
                        
                        // 根据开关状态控制电机
                        if (current_switch_level == 0) {
                            // 开关闭合（高电平）
                            gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_ENABLE_LEVEL);
                            ESP_LOGI(TAG, "Motor enabled by switch");
                        } else {
                            // 开关断开（低电平）
                            gpio_set_level(MOTOR_CONTROL_GPIO, MOTOR_CONTROL_DISABLE_LEVEL);
                            ESP_LOGI(TAG, "Motor disabled by switch");
                        }
                        
                        last_switch_level = current_switch_level;
                    }

                    // 按键输入检测逻辑
                    int current_key_level = gpio_get_level(KEY_INPUT_GPIO);
                    if (current_key_level != last_key_level) {
                        ESP_LOGI(TAG, "Key state changed: %d -> %d", last_key_level, current_key_level);
                        
                        // 发送按键事件到队列
                        KeyEvent event;
                        event.timestamp = esp_timer_get_time();
                        event.type = (current_key_level == 0) ? KEY_EVENT_PRESS : KEY_EVENT_RELEASE;
                        
                        if (xQueueSend(key_event_queue_, &event, 0) != pdTRUE) {
                            ESP_LOGW(TAG, "Key event queue full, dropping event");
                        }
                        
                        last_key_level = current_key_level;
                    }
                    
                    // 处理按键事件
                    self->ProcessKeyEvent();

                    // 开关机按键输入检测逻辑
                    int current_shutdown_level = gpio_get_level(SHUTDOWN_BUTTON_GPIO);
                    if (current_shutdown_level != last_shutdown_level) {
                        ESP_LOGI(TAG, "Shutdown button state changed: %d -> %d", last_shutdown_level, current_shutdown_level);
                        
                        // 发送开关机按键事件到队列
                        KeyEvent event;
                        event.timestamp = esp_timer_get_time();
                        event.type = (current_shutdown_level == 0) ? KEY_EVENT_PRESS : KEY_EVENT_RELEASE;
                        
                        if (xQueueSend(shutdown_event_queue_, &event, 0) != pdTRUE) {
                            ESP_LOGW(TAG, "Shutdown event queue full, dropping event");
                        }
                        
                        last_shutdown_level = current_shutdown_level;
                    }
                    
                    // 处理开关机按键事件
                    self->ProcessShutdownEvent();

                    // 处理旋转编码器
                    self->ProcessEncoder();

                    // 更新电池状态
                    self->UpdateBatteryStatus();

                    self->UpdateChargeStatus();

                    // 处理自定义输入事件
                    self->ProcessCustomInput();
                    
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
        xTaskCreatePinnedToCore(
            [](void* /*arg*/) {
                bool last_connected = WifiStation::GetInstance().IsConnected();
                ESP_LOGI(TAG, "Network monitor start, connected=%d", last_connected);
                while (true) {
                    bool connected = WifiStation::GetInstance().IsConnected();
                    // 读取当前 RSSI（仅在已连接时有效）
                    if (connected) {
                        int8_t rssi = WifiStation::GetInstance().GetRssi();
                        ESP_LOGI(TAG, "WiFi RSSI: %d dBm", rssi);
                    }
                    if (connected != last_connected) {
                        ESP_LOGI(TAG, "Network state changed: %s",
                                 connected ? "connected" : "disconnected");
                        last_connected = connected;
                    }
                    vTaskDelay(pdMS_TO_TICKS(2000)); // 2s 检测一次
                }
            },
            "net_monitor",
            2048,
            nullptr,
            4,
            nullptr,
            0
        );
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {

        static Es8388AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC,
            AUDIO_CODEC_ES8388_ADDR);
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
};

// 静态成员变量定义
QueueHandle_t ai_martube_esp32s3::key_event_queue_ = nullptr;
QueueHandle_t ai_martube_esp32s3::shutdown_event_queue_ = nullptr;

DECLARE_BOARD(ai_martube_esp32s3);
