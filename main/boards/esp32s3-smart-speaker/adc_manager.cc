#include "adc_manager.h"
#include "application.h"
#include <board.h>
#include <cmath>
#include <cstring>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "AdcManager"

// 检测时间定义（毫秒）
#define PRESSURE_DETECTION_TIME_MS 2000  // 压力检测持续时间：2秒
#define LOW_VALUE_DETECTION_TIME_MS 2000 // 低值检测持续时间：2秒

AdcManager &AdcManager::GetInstance() {
  static AdcManager instance;
  return instance;
}

bool AdcManager::Initialize() {
  if (initialized_) {
    ESP_LOGW(TAG, "AdcManager already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing AdcManager...");

  // 初始化ADC数组
  memset(pressure_adc_values_, 0, sizeof(pressure_adc_values_));

  InitializeAdc();

  // 先设置初始化状态，再启动任务
  initialized_ = true;

  // 初始化后立刻读取一次，便于快速确认链路
  int init_read_raw = -1;
  esp_err_t init_read_ret = adc_oneshot_read(
      adc1_handle_, PRESSURE_SENSOR_ADC_LEFT_CHANNEL, &init_read_raw);
  if (init_read_ret != ESP_OK) {
    ESP_LOGE(TAG, "Initial ADC read failed: %s",
             esp_err_to_name(init_read_ret));
  } else {
    ESP_LOGI(TAG, "Initial ADC read ok: Raw=%d", init_read_raw);
  }

  // 启动ADC任务
  // StartAdcTask();

  ESP_LOGI(TAG, "AdcManager initialized successfully");
  return true;
}

void AdcManager::InitializeAdc() {
  ESP_LOGI(TAG, "Initializing ADC for pressure sensor on GPIO4 (ADC1_CH3)...");

  // 初始化ADC驱动
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = ADC_UNIT_1,
  };
  esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(TAG, "ADC unit initialized successfully");

  // 配置ADC通道
  adc_oneshot_chan_cfg_t chan_config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  ret = adc_oneshot_config_channel(adc1_handle_, PRESSURE_SENSOR_ADC_LEFT_CHANNEL,
                                   &chan_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s",
             PRESSURE_SENSOR_ADC_LEFT_CHANNEL, esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(TAG, "ADC channel %d configured successfully",
           PRESSURE_SENSOR_ADC_LEFT_CHANNEL);

  // 初始化ADC校准
  adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "ADC calibration not available: %s", esp_err_to_name(ret));
    adc1_cali_handle_ = NULL;
  } else {
    ESP_LOGI(TAG, "ADC calibration initialized successfully");
  }

  ESP_LOGI(TAG, "ADC initialized for pressure sensor monitoring on GPIO4");
}

void AdcManager::ReadPressureSensorData() {
  if (!initialized_) {
    return;
  }

  int adc_value;
  esp_err_t ret =
      adc_oneshot_read(adc1_handle_, PRESSURE_SENSOR_ADC_LEFT_CHANNEL, &adc_value);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read pressure sensor ADC: %s",
             esp_err_to_name(ret));
    return;
  }

  // 直接使用原始ADC值，不进行电压转换
  current_pressure_value_ = adc_value;

  // 压力检测触发音乐播放
  static bool last_pressure_state = false;
  static int64_t pressure_start_time = 0;
  static bool pressure_triggered = false;

  if (last_pressure_state) {
    // 长时间不动检测逻辑
    CheckLongTimeNoMovement(current_pressure_value_);
  }

  // 每隔5次打印一次详细日志（便于定位问题）
  static int adc_log_counter = 0;
  adc_log_counter++;
  if (adc_log_counter >= 10) {
    ESP_LOGI(TAG, "ADC read: Raw=%d", adc_value);
    adc_log_counter = 0;
  }

  bool current_pressure_state = IsPressureDetected();

  if (current_pressure_state && !last_pressure_state) {
    // 压力开始检测，记录开始时间
    pressure_start_time = esp_timer_get_time();
    pressure_triggered = false;
    ESP_LOGI(TAG, "Pressure detection started");
  } else if (current_pressure_state && last_pressure_state) {
    // 压力持续检测中，检查是否超过2秒
    int64_t current_time = esp_timer_get_time();
    int64_t duration_ms =
        (current_time - pressure_start_time) / 1000; // 转换为毫秒

    if (duration_ms >= PRESSURE_DETECTION_TIME_MS && !pressure_triggered) {
      ESP_LOGI(TAG,
               "Pressure detected for %ld ms! Triggering music playback...",
               (long)duration_ms);
      // 触发音乐播放
      TriggerMusicPlayback();
      pressure_triggered = true; // 防止重复触发
    }
  } else if (!current_pressure_state && last_pressure_state) {
    // 压力结束检测
    ESP_LOGI(TAG, "Pressure detection ended");
    pressure_triggered = false;
  }

  last_pressure_state = current_pressure_state;

  // ADC值小于100时的暂停检测（也需要持续2秒）
  static int64_t low_value_start_time = 0;
  static bool low_value_triggered = false;

  if (adc_value < 100) {
    if (low_value_start_time == 0) {
      // 第一次检测到小于100的值，记录开始时间
      low_value_start_time = esp_timer_get_time();
      low_value_triggered = false;
      ESP_LOGI(TAG, "ADC low value detection started (value: %d)", adc_value);
    } else {
      // 持续检测小于100的值，检查是否超过2秒
      int64_t current_time = esp_timer_get_time();
      int64_t duration_ms =
          (current_time - low_value_start_time) / 1000; // 转换为毫秒

      if (duration_ms >= LOW_VALUE_DETECTION_TIME_MS && !low_value_triggered) {
        ESP_LOGI(TAG,
                 "ADC low value detected for %ld ms! (value: %d) Triggering music pause...",
                 (long)duration_ms, adc_value);
        TriggerMusicPauseback();
        low_value_triggered = true; // 防止重复触发
      }
    }
  } else {
    // ADC值大于等于100，重置低值检测
    if (low_value_start_time != 0) {
      ESP_LOGI(TAG, "ADC low value detection ended (value: %d)", adc_value);
      low_value_start_time = 0;
      low_value_triggered = false;
    }
  }
}

void AdcManager::StartAdcTask() {
  if (!initialized_) {
    ESP_LOGE(TAG, "AdcManager not initialized");
    return;
  }

  BaseType_t ret =
      xTaskCreate(AdcTask, "adc_task", 4096, this, 2, &adc_task_handle_);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create ADC task");
  } else {
    ESP_LOGI(TAG, "ADC task created successfully");
  }
}

void AdcManager::StopAdcTask() {
  if (adc_task_handle_) {
    vTaskDelete(adc_task_handle_);
    adc_task_handle_ = nullptr;
  }
}

void AdcManager::AdcTask(void *pvParameters) {
  AdcManager *manager = static_cast<AdcManager *>(pvParameters);
  ESP_LOGI(TAG, "ADC task started");

  while (true) {
    if (manager->initialized_) {
      manager->ReadPressureSensorData();
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms间隔
  }
}

int AdcManager::GetCurrentPressureValue() const {
  return current_pressure_value_;
}

const int *AdcManager::GetPressureAdcValues() const {
  return pressure_adc_values_;
}

size_t AdcManager::GetPressureSampleCount() const {
  return (pressure_data_index_ == 0) ? kPressureAdcDataCount
                                     : pressure_data_index_;
}

bool AdcManager::IsPressureDetected() const {
  if (!initialized_) {
    return false;
  }
  return current_pressure_value_ > 1000; // 压力阈值：100
}

bool AdcManager::IsLightPressure() const {
  if (!initialized_) {
    return false;
  }
  return current_pressure_value_ > 500; // 轻压阈值：500
}

void AdcManager::CheckLongTimeNoMovement(int adc_value) {
  uint32_t current_time = esp_timer_get_time() / 1000000; // 转换为秒

  // 计算ADC值变化
  int adc_change = abs(adc_value - last_stable_value_);

  if (adc_change > kMovementThreshold) {
    // 有显著变化，重置不动检测
    last_stable_value_ = adc_value;
    no_movement_start_time_ = current_time;
    is_no_movement_detected_ = false;
  } else {
    // 变化很小，检查是否长时间不动
    if (no_movement_start_time_ == 0) {
      no_movement_start_time_ = current_time;
    }

    uint32_t no_movement_duration = current_time - no_movement_start_time_;
    if (no_movement_duration >= kLongTimeThreshold &&
        !is_no_movement_detected_) {
      is_no_movement_detected_ = true;
      ESP_LOGW(TAG,
               "Long time no movement detected! Duration: %lu seconds, ADC: %d",
               no_movement_duration, adc_value);

      // 停止音乐播放和语音交互
      TriggerMusicPauseback();
    }
  }
}

bool AdcManager::IsLongTimeNoMovement() const {
  if (!initialized_) {
    return false;
  }
  return is_no_movement_detected_;
}

uint32_t AdcManager::GetNoMovementDuration() const {
  if (!initialized_ || no_movement_start_time_ == 0) {
    return 0;
  }

  uint32_t current_time = esp_timer_get_time() / 1000000;
  return current_time - no_movement_start_time_;
}

void AdcManager::TriggerMusicPauseback() {
  ESP_LOGI(TAG, "Triggering music pauseback");
  auto music = Board::GetInstance().GetMusic();
  if (!music) {
    ESP_LOGI(TAG, "No music player found");
    return;
  }
  
  // 检查音乐状态，避免重复操作
  if (!music->IsPlaying() && !music->IsPaused()) {
    ESP_LOGI(TAG, "Music is not playing or paused, skipping pause operation");
    return;
  }
  
  music->PauseSong();

  // 停止语音交互
  auto &app = Application::GetInstance();
  app.GetAudioService().EnableWakeWordDetection(false);
  ESP_LOGI(TAG, "Stopped wake word detection due to long time no movement");
}

void AdcManager::TriggerMusicPlayback() {
  ESP_LOGI(TAG, "Triggering music playback");
  
  // 确保音频输出已启用
  auto& board = Board::GetInstance();
  auto codec = board.GetAudioCodec();
  if (!codec) {
    ESP_LOGE(TAG, "Audio codec not available");
    return;
  }
  
  codec->EnableOutput(true);
  ESP_LOGI(TAG, "Audio output enabled");
  
  // 通过Board接口获取音乐播放器并触发播放
  auto music = board.GetMusic();
  if (!music) {
    ESP_LOGI(TAG, "No music player found");
    return;
  }
  if (music->IsPlaying()) {
    ESP_LOGI(TAG, "Music is already playing");
    return;
  }
  if (music->IsDownloading()) {
    ESP_LOGI(TAG, "Music is already downloading");
    return;
  }
  if (music->IsPaused()) {
    ESP_LOGI(TAG, "Music is already paused");
    music->ResumeSong();
    return;
  }
  auto song_name = "稻香";
  auto artist_name = "";
  if (!music->Download(song_name, artist_name)) {
    ESP_LOGI(TAG, "获取音乐资源失败");
    return;
  }

  auto download_result = music->GetDownloadResult();
  ESP_LOGI(TAG, "Music details result: %s", download_result.c_str());
}
