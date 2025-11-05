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

#define TAG "ai_martube_esp32s3"
// 旋转编码器轻量去抖窗口（微秒）
#define ENCODER_DEBOUNCE_US 2000

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

    bool to_open_audio = false;
    
    // 按键相关变量
    static QueueHandle_t key_event_queue_;
    int64_t key_press_time_;
    bool key_is_pressed_;
    uint8_t light_mode_;  // 灯光模式：0=灭, 1=30%亮度, 2=100%亮度
    std::function<void()> on_short_press_callback_;  // 短按事件回调
    std::function<void()> on_long_press_callback_;   // 长按事件回调

    // 开关机按键相关变量
    static QueueHandle_t shutdown_event_queue_;
    int64_t shutdown_press_time_;
    bool shutdown_is_pressed_;
    std::function<void()> on_shutdown_long_press_callback_;  // 关机长按事件回调

    // 旋转编码器相关变量
    int encoder_a_last_state_;
    int encoder_b_last_state_;
    int current_volume_;  // 当前音量 (0-100)
    std::function<void(int)> on_volume_change_callback_;  // 音量变化回调
    int64_t last_a_change_time_us_ = 0;
    int64_t last_b_change_time_us_ = 0;
    
    // 模式标志：false=AI模式，true=蓝牙模式
    bool bluetooth_mode_ = false;

    // 电池电量相关变量
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    float battery_voltage_;
    int battery_percentage_;
    int64_t last_battery_check_time_;

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


    // 初始化音频切换IO：配置为输出模式，上电后设置为高电平（ESP32S3使用）
    void InitializeAudioSwitch()
    {
        gpio_config_t audio_switch_init_struct = {0};

        audio_switch_init_struct.mode = GPIO_MODE_OUTPUT;
        audio_switch_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        audio_switch_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        audio_switch_init_struct.pin_bit_mask = 1ull << AUDIO_SWITCH_GPIO;
        ESP_ERROR_CHECK(gpio_config(&audio_switch_init_struct));
        
        // 上电后设置为高电平，使用ESP32S3音频
        gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
        ESP_LOGI(TAG, "Audio switch initialized on GPIO %d, set to ESP32S3 mode (HIGH)", AUDIO_SWITCH_GPIO);
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

    // 初始化功放控制IO：将GPIO配置为输出模式，初始状态为禁用功放
    void InitializePowerAmplifier()
    {
        gpio_config_t pa_init_struct = {0};

        pa_init_struct.mode = GPIO_MODE_OUTPUT;
        pa_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        pa_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pa_init_struct.pin_bit_mask = 1ull << AUDIO_CODEC_PA_GPIO;
        ESP_ERROR_CHECK(gpio_config(&pa_init_struct));
        
        // 初始状态为禁用功放
        gpio_set_level(AUDIO_CODEC_PA_GPIO, 1);
        ESP_LOGI(TAG, "Power amplifier control initialized on GPIO %d, status: LOW (DISABLED)", AUDIO_CODEC_PA_GPIO);
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
                    ESP_LOGI(TAG, "Key pressed at %lld", key_press_time_);
                }
            } else if (event.type == KEY_EVENT_RELEASE) {
                if (key_is_pressed_) {
                    key_is_pressed_ = false;
                    int64_t press_duration = current_time - key_press_time_;
                    ESP_LOGI(TAG, "Key released, duration: %lld us", press_duration);
                    
                    if (press_duration >= KEY_LONG_PRESS_TIME_MS * 1000) {
                        // 长按
                        ESP_LOGI(TAG, "Long press detected");
                        if (on_long_press_callback_) {
                            on_long_press_callback_();
                        }
                    } else if (press_duration >= KEY_DEBOUNCE_TIME_MS * 1000) {
                        // 短按
                        ESP_LOGI(TAG, "Short press detected");
                        if (on_short_press_callback_) {
                            on_short_press_callback_();
                        }
                    }
                }
            }
        }
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

    // 短按处理：循环切换灯光模式
    void OnShortPress()
    {
        light_mode_ = (light_mode_ + 1) % 3;  // 0->1->2->0
        ESP_LOGI(TAG, "Short press: switch light mode to %u", light_mode_);
        SetLightMode(light_mode_);
    }

    // 长按处理
    void OnLongPress()
    {
        // 长切换模式：AI 模式 <-> 蓝牙模式，并发送串口命令
        // AI -> 蓝牙：A5 00 02 05
        // 蓝牙 -> AI：A5 00 02 06
        if (!bluetooth_mode_) {
            // 切到蓝牙模式
            gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
            EnablePowerAmplifier();
            uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x05};
            if (uart_comm_ && uart_comm_->IsReady()) {
                uart_comm_->Send(cmd, sizeof(cmd));
            } else {
                ESP_LOGW(TAG, "UART not ready, skip sending BT command");
            }
            ESP_LOGI(TAG, "Long press: switch to Bluetooth mode, sent A5 00 02 07");
            bluetooth_mode_ = true;
        } else {
            // 切到 AI 模式
            gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
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
        gpio_set_level(POWER_ON_CONTROL_GPIO, 1);
        ESP_LOGI(TAG, "Power control initialized on GPIO %d, status: HIGH (ON)", POWER_ON_CONTROL_GPIO);
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
                    ESP_LOGI(TAG, "Shutdown button pressed at %lld", shutdown_press_time_);
                }
            } else if (event.type == KEY_EVENT_RELEASE) {
                if (shutdown_is_pressed_) {
                    shutdown_is_pressed_ = false;
                    int64_t press_duration = current_time - shutdown_press_time_;
                    ESP_LOGI(TAG, "Shutdown button released, duration: %lld us", press_duration);
                    
                    if (press_duration >= SHUTDOWN_LONG_PRESS_TIME_MS * 1000) {
                        // 长按 - 执行关机
                        ESP_LOGI(TAG, "Shutdown long press detected");
                        if (on_shutdown_long_press_callback_) {
                            on_shutdown_long_press_callback_();
                        }
                    }
                    // 短按不处理
                }
            }
        }
    }

    // 关机长按处理
    void OnShutdownLongPress()
    {
        ESP_LOGI(TAG, "Shutdown command triggered by long press");
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
        int64_t now_us = esp_timer_get_time();

        // A 相去抖：仅在超过去抖窗口后处理边沿
        if (a_now != encoder_a_last_state_) {
            if (now_us - last_a_change_time_us_ >= ENCODER_DEBOUNCE_US) {
                last_a_change_time_us_ = now_us;
                // 仅在 A 下降沿判定方向（如需上升沿同样处理可扩展）
                if (a_now == 0) {
                    // 延时采样，避开抖动窗口
                    esp_rom_delay_us(200);
                    int b_stable = gpio_get_level(ENCODER_B_GPIO);

                    // 方向判定：使用 B 的稳定值与 A 的新状态比较
                    bool clockwise = (b_stable == a_now);
                    if (clockwise) {
                        // 顺时针：蓝牙模式发增大音量指令；AI模式增加本地音量
                        if (bluetooth_mode_) {
                            uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x01};
                            if (uart_comm_ && uart_comm_->IsReady()) {
                                uart_comm_->Send(cmd, sizeof(cmd));
                            } else {
                                ESP_LOGW(TAG, "UART not ready, skip sending BT + cmd");
                            }
                        } else {
                            if (current_volume_ < 100) {
                                current_volume_ += 5;
                                if (current_volume_ > 100) current_volume_ = 100;
                                ESP_LOGI(TAG, "Volume increased to %d%%", current_volume_);
                                if (on_volume_change_callback_) {
                                    on_volume_change_callback_(current_volume_);
                                }
                            }
                        }
                    } else {
                        // 逆时针：蓝牙模式发减小音量指令；AI模式降低本地音量
                        if (bluetooth_mode_) {
                            uint8_t cmd[4] = {0xA5, 0x00, 0x02, 0x02};
                            if (uart_comm_ && uart_comm_->IsReady()) {
                                uart_comm_->Send(cmd, sizeof(cmd));
                            } else {
                                ESP_LOGW(TAG, "UART not ready, skip sending BT - cmd");
                            }
                        } else {
                            if (current_volume_ > 0) {
                                current_volume_ -= 5;
                                if (current_volume_ < 0) current_volume_ = 0;
                                ESP_LOGI(TAG, "Volume decreased to %d%%", current_volume_);
                                if (on_volume_change_callback_) {
                                    on_volume_change_callback_(current_volume_);
                                }
                            }
                        }
                    }
                }
                encoder_a_last_state_ = a_now;
            } else {
                // 去抖窗口内变化，直接更新上一状态但不动作
                encoder_a_last_state_ = a_now;
            }
        }

        // B 相去抖：记录最后变化时间（如需在 B 边沿判定方向可扩展）
        if (b_now != encoder_b_last_state_) {
            if (now_us - last_b_change_time_us_ >= ENCODER_DEBOUNCE_US) {
                last_b_change_time_us_ = now_us;
            }
            encoder_b_last_state_ = b_now;
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

        // 校准ADC
        adc_cali_curve_fitting_config_t cali_config = {};
        cali_config.unit_id = ADC_UNIT_1;
        cali_config.atten = ADC_ATTEN_DB_12;
        cali_config.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_));
        
        // 初始化电池状态
        battery_voltage_ = 0.0f;
        battery_percentage_ = 0;
        last_battery_check_time_ = 0;
        
        ESP_LOGI(TAG, "Battery monitor initialized on GPIO %d (ADC_CH%d)", 
                 BATTERY_ADC_GPIO, BATTERY_ADC_CHANNEL);
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
        float voltage = (float)voltage_mv / 1000.0f;  // 转换为伏特
        
        // 考虑分压器比例
        float battery_voltage = voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;
        
        return battery_voltage;
    }

    // 计算电池电量百分比
    int CalculateBatteryPercentage(float voltage)
    {
        if (voltage >= BATTERY_FULL_VOLTAGE) {
            return 100;
        } else if (voltage <= BATTERY_EMPTY_VOLTAGE) {
            return 0;
        } else {
            // 线性插值计算百分比
            float percentage = ((voltage - BATTERY_EMPTY_VOLTAGE) / 
                              (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0f;
            return (int)percentage;
        }
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

        // 添加音频切换工具
        mcp_server.AddTool("self.audio_switch.set_esp32s3",
                           "切换到ESP32S3音频模式",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
                               ESP_LOGI(TAG, "Audio switch set to ESP32S3 mode");
                               return true;
                           });

        mcp_server.AddTool("self.audio_switch.set_bluetooth",
                           "切换到经典蓝牙音频模式",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                               ESP_LOGI(TAG, "Audio switch set to Bluetooth mode");
                               return true;
                           });

        mcp_server.AddTool("self.audio_switch.get_status",
                           "获取音频切换状态",
                           PropertyList(),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               int level = gpio_get_level(AUDIO_SWITCH_GPIO);
                               return (level == AUDIO_SWITCH_ESP32S3_LEVEL) ? "ESP32S3模式" : "蓝牙模式";
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
        InitializeAudioSwitch();
        InitializeMotor();
        InitializePowerAmplifier();
        InitializePowerControl();
        InitializeBatteryMonitor();
        // InitializeSwitchInput();
        InitializeKeyInput();
        InitializeShutdownButton();
        InitializeEncoder();
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
        pwm_led_controller_ = new PwmLedController(pwm_led_);
        
        // 设置按键回调
        SetKeyCallbacks(
            [this]() { OnShortPress(); },  // 短按回调
            [this]() { OnLongPress(); }    // 长按回调
        );
        
        // 设置开关机按键回调
        SetShutdownCallback(
            [this]() { OnShutdownLongPress(); }  // 关机长按回调
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
                    gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_ESP32S3_LEVEL);
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
                
                while (true) {
                    // 音频控制逻辑
                    DeviceState cur = app.GetDeviceState();
                    
                    // 根据设备状态切换音频模式
                    if (cur != last) {
                        if (cur == kDeviceStateIdle) {
                            // 进入待机状态时，切换到蓝牙模式
                            gpio_set_level(AUDIO_SWITCH_GPIO, AUDIO_SWITCH_BLUETOOTH_LEVEL);
                            // gpio_set_level(AUDIO_SWITCH_INPUT_GPIO, AUDIO_SWITCH_INPUT_BLUETOOTH_LEVEL);
                            ESP_LOGI(TAG, "Device state changed to idle, switching to Bluetooth audio mode");
                            self->DisablePowerAmplifier();
                        }
                        // else if (cur == kDeviceStateSpeaking) {
                        //     auto* audio_codec = self->GetAudioCodec();  // 使用board的GetAudioCodec方法

                        //     audio_codec->EnableOutput(false);
                        //     vTaskDelay(pdMS_TO_TICKS(10));  // 短暂延迟
                        //     ESP_LOGI(TAG, "Output channel disabled");
                        //     audio_codec->EnableOutput(true);
                        //     vTaskDelay(pdMS_TO_TICKS(50));  // 给输出通道初始化时间

                        // }
                        
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
                    
                    vTaskDelay(pdMS_TO_TICKS(10));  // 减少延迟以提高编码器响应速度
                }
            },
            "dev_state_monitor",
            4096,
            this,
            5,
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
