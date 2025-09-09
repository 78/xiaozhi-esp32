#include "assets.h"
#include "adc_manager.h"
#include "button_manager.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "esplog_display.h"
#include "esp32_music.h"
#include "gpio_manager.h"
#include "imu_manager.h"
#include "led/single_led.h"
#include "tools_manager.h"
#include "wifi_board.h"
#include "wifi_manager.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <string>

#define TAG "SmartSpeaker"

class Esp32s3SmartSpeaker : public WifiBoard {
private:
  // I2C总线句柄
  i2c_master_bus_handle_t codec_i2c_bus_;

  void InitializeManagers() {
    ESP_LOGI(TAG, "Initializing managers...");
    
    // 初始化各个管理器（Initialize内部会自动启动任务）
    AdcManager::GetInstance().Initialize();
    ImuManager::GetInstance().Initialize();
    ButtonManager::GetInstance().Initialize();
    GpioManager::GetInstance().Initialize();
    ToolsManager::GetInstance().Initialize();
    WifiManager::GetInstance().Initialize();
    
    ESP_LOGI(TAG, "All managers initialized successfully");
  }
  
  void InitializeCodecI2c() {
    ESP_LOGI(TAG, "Initializing ES8311 codec I2C...");
    
    // 初始化I2C外设
    i2c_master_bus_config_t i2c_bus_cfg = {
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
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    
    // 探测I2C设备
    if (i2c_master_probe(codec_i2c_bus_, AUDIO_CODEC_ES8311_ADDR, 1000) != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 not found on I2C bus, audio may not work");
    } else {
        ESP_LOGI(TAG, "ES8311 codec detected on I2C bus");
    }
  }


public:
  Esp32s3SmartSpeaker() {
    ESP_LOGI(TAG, "Initializing ESP32-S3 Smart Speaker");

    // 初始化音乐播放器
    music_ = new Esp32Music();
    ESP_LOGI(TAG, "Music player initialized");

    // 初始化I2C总线
    //InitializeCodecI2c();
    
    // 初始化各个管理器
    InitializeManagers();

    ESP_LOGI(TAG, "ESP32-S3 Smart Speaker initialized successfully");
  }

  virtual ~Esp32s3SmartSpeaker() = default;

  virtual std::string GetBoardType() override {
    return std::string("esp32s3-smart-speaker");
  }

  virtual AudioCodec *GetAudioCodec() override {
    static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_MIC_I2S_DIN);
    return &audio_codec;
  }

  virtual Display *GetDisplay() override {
    static EspLogDisplay display;
    return &display;
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual std::string GetBoardJson() override {
    char json_buffer[2048];
    snprintf(json_buffer, sizeof(json_buffer),
        "{"
        "\"board_type\":\"esp32s3-smart-speaker\","
        "\"version\":\"%s\","
        "\"features\":[\"audio\",\"imu\",\"pressure\",\"led_ring\",\"fan\",\"relay\",\"status_led\"],"
        "\"audio_codec\":\"NoAudioCodec\","
        "\"audio_method\":\"i2s_standard\","
        "\"microphone\":\"NoAudioCodec\","
        "\"speaker\":\"NoAudioCodec\","
        "\"imu_initialized\":%s,"
        "\"pressure_sensor_initialized\":%s,"
        "\"pressure_sensor\":{\"current_value\":%d,\"adc_channel\":%d,\"sample_count\":%zu},"
        "\"imu_sensor\":{\"type\":\"MPU6050\",\"initialized\":%s,\"status\":\"unknown\"}"
        "}",
        SMART_SPEAKER_VERSION,
        ImuManager::GetInstance().IsInitialized() ? "true" : "false",
        AdcManager::GetInstance().IsInitialized() ? "true" : "false",
        AdcManager::GetInstance().GetCurrentPressureValue(),
        PRESSURE_SENSOR_ADC_CHANNEL,
        AdcManager::GetInstance().GetPressureSampleCount(),
        ImuManager::GetInstance().GetImuSensor() && ImuManager::GetInstance().GetImuSensor()->IsInitialized() ? "true" : "false"
    );
    return std::string(json_buffer);
  }

  virtual Assets *GetAssets() override {
    static Assets assets(std::string(ASSETS_XIAOZHI_WAKENET_SMALL));
    return &assets;
  }
};

DECLARE_BOARD(Esp32s3SmartSpeaker);
