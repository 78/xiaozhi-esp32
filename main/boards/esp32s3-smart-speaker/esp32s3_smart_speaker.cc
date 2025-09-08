#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "settings.h"
#include "mpu6050_sensor.h"
#include "assets.h"
#include "esplog_display.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>

#define TAG "SmartSpeaker"

class Esp32s3SmartSpeaker : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    i2c_master_bus_handle_t imu_i2c_bus_;
    
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Button touch_button_;
    
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    bool pressure_sensor_initialized_ = false;
    
    // MPU6050传感器
    std::unique_ptr<Mpu6050Sensor> mpu6050_sensor_;
    bool imu_initialized_ = false;

    void InitializeI2c() {
        // ES8311编解码器I2C总线
        i2c_master_bus_config_t codec_i2c_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
                .allow_pd = false,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&codec_i2c_cfg, &codec_i2c_bus_));
        
        // IMU传感器I2C总线
        i2c_master_bus_config_t imu_i2c_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = IMU_I2C_SDA_PIN,
            .scl_io_num = IMU_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
                .allow_pd = false,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&imu_i2c_cfg, &imu_i2c_bus_));
        
        // IO扩展器I2C总线 (移除，因为ESP32-S3只有2个I2C)
        // i2c_master_bus_config_t io_i2c_cfg = {
        //     .i2c_port = I2C_NUM_2,
        //     .sda_io_num = IO_EXPANDER_SDA_PIN,
        //     .scl_io_num = IO_EXPANDER_SCL_PIN,
        //     .clk_source = I2C_CLK_SRC_DEFAULT,
        //     .glitch_ignore_cnt = 7,
        //     .intr_priority = 0,
        //     .trans_queue_depth = 0,
        //     .flags = {
        //         .enable_internal_pullup = 1,
        //     },
        // };
        // ESP_ERROR_CHECK(i2c_new_master_bus(&io_i2c_cfg, &io_expander_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([]() {
            // 简化按钮处理，避免复杂的应用状态管理
            ESP_LOGI(TAG, "Boot button clicked");
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            ESP_LOGI(TAG, "Volume up to %d", volume);
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            ESP_LOGI(TAG, "Volume set to maximum");
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            ESP_LOGI(TAG, "Volume down to %d", volume);
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            ESP_LOGI(TAG, "Volume muted");
        });

        touch_button_.OnClick([]() {
            ESP_LOGI(TAG, "Touch button pressed");
        });
    }

    void InitializeAdc() {
        // 初始化ADC驱动 (参考其他开发板的实现)
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle_));
        
        // 配置ADC通道
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_3, &chan_config));
        
        // 初始化ADC校准 (可选)
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ADC calibration not available");
            adc1_cali_handle_ = NULL;
        }
        
        pressure_sensor_initialized_ = true;
        ESP_LOGI(TAG, "ADC initialized for pressure sensor and battery monitoring");
    }

    void InitializeImu() {
        ESP_LOGI(TAG, "Initializing MPU6050 IMU sensor...");
        
        // 初始化MPU6050传感器
        mpu6050_sensor_ = std::make_unique<Mpu6050Sensor>(imu_i2c_bus_);
        
        if (mpu6050_sensor_) {
            // 先验证设备ID
            uint8_t device_id;
            if (mpu6050_sensor_->GetDeviceId(&device_id)) {
                ESP_LOGI(TAG, "MPU6050 device ID: 0x%02X", device_id);
                if (device_id == MPU6050_WHO_AM_I_VAL) {
                    // 初始化传感器
                    if (mpu6050_sensor_->Initialize(ACCE_FS_4G, GYRO_FS_500DPS)) {
                        // 唤醒传感器
                        if (mpu6050_sensor_->WakeUp()) {
                            imu_initialized_ = true;
                            ESP_LOGI(TAG, "MPU6050 sensor initialized successfully (ID: 0x%02X)", device_id);
                            
                            // 启动传感器数据读取任务
                            StartImuDataTask();
                        } else {
                            ESP_LOGE(TAG, "Failed to wake up MPU6050");
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to initialize MPU6050");
                    }
                } else {
                    ESP_LOGE(TAG, "MPU6050 device ID mismatch: expected 0x%02X, got 0x%02X", 
                            MPU6050_WHO_AM_I_VAL, device_id);
                }
            } else {
                ESP_LOGE(TAG, "Failed to read MPU6050 device ID - check hardware connections");
                ESP_LOGE(TAG, "I2C SDA: GPIO%d, SCL: GPIO%d", IMU_I2C_SDA_PIN, IMU_I2C_SCL_PIN);
            }
        } else {
            ESP_LOGE(TAG, "Failed to create MPU6050 sensor instance");
        }
        
        if (!imu_initialized_) {
            ESP_LOGW(TAG, "IMU sensor initialization failed - continuing without IMU");
        }
    }

    void InitializeGpio() {
        // 初始化GPIO输出
        gpio_config_t io_conf = {};
        
        // LED灯环控制
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << LED_RING_GPIO);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        
        // 状态指示灯
        io_conf.pin_bit_mask = (1ULL << STATUS_LED_GPIO);
        gpio_config(&io_conf);
        
        ESP_LOGI(TAG, "GPIO initialized");
    }

    void InitializeTools() {
        // 简化的MCP工具注册
        ESP_LOGI(TAG, "MCP tools initialized (simplified)");
    }

    void StartImuDataTask() {
        // 创建IMU数据读取任务
        BaseType_t ret = xTaskCreate(
            ImuDataTask,           // 任务函数
            "imu_data_task",       // 任务名称
            4096,                  // 栈大小
            this,                  // 任务参数
            5,                     // 任务优先级
            nullptr                // 任务句柄
        );
        
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create IMU data task");
        } else {
            ESP_LOGI(TAG, "IMU data task created successfully");
        }
    }

    static void ImuDataTask(void* pvParameters) {
        Esp32s3SmartSpeaker* board = static_cast<Esp32s3SmartSpeaker*>(pvParameters);
        
        mpu6050_acce_value_t acce;
        mpu6050_gyro_value_t gyro;
        mpu6050_temp_value_t temp;
        complimentary_angle_t angle;
        
        ESP_LOGI(TAG, "IMU data task started");
        
        while (true) {
            if (board->mpu6050_sensor_ && board->imu_initialized_) {
                // 读取加速度计数据
                if (board->mpu6050_sensor_->GetAccelerometer(&acce)) {
                    ESP_LOGI(TAG, "Accelerometer - X:%.2f, Y:%.2f, Z:%.2f", 
                            acce.acce_x, acce.acce_y, acce.acce_z);
                }
                
                // 读取陀螺仪数据
                if (board->mpu6050_sensor_->GetGyroscope(&gyro)) {
                    ESP_LOGI(TAG, "Gyroscope - X:%.2f, Y:%.2f, Z:%.2f", 
                            gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
                }
                
                // 读取温度数据
                if (board->mpu6050_sensor_->GetTemperature(&temp)) {
                    ESP_LOGI(TAG, "Temperature: %.2f°C", temp.temp);
                }
                
                // 计算姿态角
                if (board->mpu6050_sensor_->ComplimentaryFilter(&acce, &gyro, &angle)) {
                    ESP_LOGI(TAG, "Attitude - Pitch:%.2f°, Roll:%.2f°, Yaw:%.2f°", 
                            angle.pitch, angle.roll, angle.yaw);
                }
            }
            
            // 每500ms读取一次
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    void InitializeDefaultWifi() {
        // 配置WiFi参数到NVS
        Settings wifi_settings("wifi", true);
        
        // 设置不记住BSSID (不区分MAC地址)
        wifi_settings.SetInt("remember_bssid", 0);
        
        // 设置最大发射功率
        wifi_settings.SetInt("max_tx_power", 0);
        
        // 禁用AP配网模式
        wifi_settings.SetInt("force_ap", 0);
        
        // 添加默认WiFi配置
        auto& wifi_station = WifiStation::GetInstance();
        wifi_station.AddAuth("xoxo", "12340000");
        
        ESP_LOGI(TAG, "Default WiFi credentials added: SSID=xoxo, Password=12340000");
        ESP_LOGI(TAG, "WiFi configured to not distinguish MAC addresses (remember_bssid=0)");
        ESP_LOGI(TAG, "AP configuration mode disabled");
    }

public:
    Esp32s3SmartSpeaker() : 
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO) {
        
        ESP_LOGI(TAG, "Initializing ESP32-S3 Smart Speaker");
        
        InitializeI2c();
        InitializeButtons();
        InitializeAdc();
        InitializeImu();
        InitializeGpio();
        InitializeTools();
        InitializeDefaultWifi();
        
        ESP_LOGI(TAG, "ESP32-S3 Smart Speaker initialized successfully");
    }

    virtual ~Esp32s3SmartSpeaker() = default;

    virtual std::string GetBoardType() override {
        return "esp32s3-smart-speaker";
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            // 扬声器（标准 I2S 输出）
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            // 麦克风（标准 I2S 输入，单声道）
            AUDIO_MIC_I2S_BCLK,
            AUDIO_MIC_I2S_WS,
            AUDIO_MIC_I2S_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        static EspLogDisplay display;
        return &display;
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual std::string GetBoardJson() override {
        std::string json = R"({"board_type":"esp32s3-smart-speaker",)";
        json += R"("version":")" + std::string(SMART_SPEAKER_VERSION) + R"(",)";
        json += R"("features":["audio","imu","pressure","led_ring","fan","relay","status_led"],)";
        json += R"("audio_codec":"NoAudioCodecSimplex",)";
        json += R"("audio_method":"i2s_standard",)";
        json += R"("microphone":"INMP441_I2S",)";
        json += R"("speaker":"I2S_Standard",)";
        json += R"("imu_initialized":)" + std::string(imu_initialized_ ? "true" : "false") + R"(,)";
        json += R"("pressure_sensor_initialized":)" + std::string(pressure_sensor_initialized_ ? "true" : "false") + R"(,)";
        
        // 添加MPU6050传感器状态信息
        if (mpu6050_sensor_) {
            json += R"("imu_sensor":{)";
            json += R"("type":"MPU6050",)";
            json += R"("initialized":)" + std::string(mpu6050_sensor_->IsInitialized() ? "true" : "false") + R"(,)";
            json += R"("status":)" + mpu6050_sensor_->GetStatusJson();
            json += R"(})";
        } else {
            json += R"("imu_sensor":{"type":"MPU6050","initialized":false,"status":"not_created"})";
        }
        
        json += R"(})";
        return json;
    }

    virtual Assets* GetAssets() override {
        static Assets assets(ASSETS_XIAOZHI_WAKENET_SMALL);
        return &assets;
    }
};

DECLARE_BOARD(Esp32s3SmartSpeaker);
