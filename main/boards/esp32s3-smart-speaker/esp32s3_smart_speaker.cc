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
    return;
  }


public:
  Esp32s3SmartSpeaker() {
    ESP_LOGI(TAG, "Initializing ESP32-S3 Smart Speaker");

    // 初始化音乐播放器
    music_ = new Esp32Music();
    ESP_LOGI(TAG, "Music player initialized");

    // 初始化I2C总线
    InitializeCodecI2c();
    
    // 初始化各个管理器
    InitializeManagers();

    ESP_LOGI(TAG, "ESP32-S3 Smart Speaker initialized successfully");
  }

  virtual ~Esp32s3SmartSpeaker() = default;

  virtual std::string GetBoardType() override {
    return std::string("esp32s3-smart-speaker");
  }

  virtual AudioCodec *GetAudioCodec() override {
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
    
    // 安全地获取管理器状态，避免在初始化过程中访问
    bool imu_initialized = false;
    bool adc_initialized = false;
    int pressure_value = 0;
    size_t pressure_sample_count = 0;
    bool imu_sensor_initialized = false;
    
    try {
      auto& imu_manager = ImuManager::GetInstance();
      imu_initialized = imu_manager.IsInitialized();
      
      auto& adc_manager = AdcManager::GetInstance();
      adc_initialized = adc_manager.IsInitialized();
      pressure_value = adc_manager.GetCurrentPressureValue();
      pressure_sample_count = adc_manager.GetPressureSampleCount();
      
      auto imu_sensor = imu_manager.GetImuSensor();
      imu_sensor_initialized = imu_sensor && imu_sensor->IsInitialized();
    } catch (...) {
      ESP_LOGW(TAG, "Error accessing managers in GetBoardJson, using default values");
    }
    
    snprintf(json_buffer, sizeof(json_buffer),
        "{"
        "\"board_type\":\"esp32s3-smart-speaker\","
        "\"version\":\"%s\","
        "\"features\":[\"audio\",\"imu\",\"pressure\",\"led_ring\",\"fan\",\"relay\",\"status_led\"],"
        "\"audio_codec\":\"NoAudioCodecSimplex\","
        "\"audio_method\":\"i2s_standard\","
        "\"microphone\":\"INMP441_I2S\","
        "\"speaker\":\"NoAudioCodec\","
        "\"imu_initialized\":%s,"
        "\"pressure_sensor_initialized\":%s,"
        "\"pressure_sensor\":{\"current_value\":%d,\"adc_channel\":%d,\"sample_count\":%u},"
        "\"imu_sensor\":{\"type\":\"MPU6050\",\"initialized\":%s,\"status\":\"unknown\"}"
        "}",
        SMART_SPEAKER_VERSION,
        imu_initialized ? "true" : "false",
        adc_initialized ? "true" : "false",
        pressure_value,
        PRESSURE_SENSOR_ADC_CHANNEL,
        (unsigned int)pressure_sample_count,
        imu_sensor_initialized ? "true" : "false"
    );
    
    ESP_LOGI(TAG, "GetBoardJson completed successfully");
    return std::string(json_buffer);
  }

  virtual Assets *GetAssets() override {
    static Assets assets(std::string(ASSETS_XIAOZHI_WAKENET_SMALL));
    return &assets;
  }
};

DECLARE_BOARD(Esp32s3SmartSpeaker);
