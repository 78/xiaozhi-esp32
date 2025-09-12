#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include "config.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AdcManager {
public:
    static AdcManager& GetInstance();
    
    // 初始化ADC系统
    bool Initialize();
    
    // 读取压感传感器数据
    void ReadPressureSensorData();
    
    // 获取当前压感值
    int GetCurrentPressureValue() const;
    
    // 获取ADC原始值数组
    const int* GetPressureAdcValues() const;
    
    // 获取有效样本数量
    size_t GetPressureSampleCount() const;
    
    // 启动/停止ADC任务
    void StartAdcTask();
    void StopAdcTask();
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }
    
    // 基于ADC值判断压力状态
    bool IsPressureDetected() const;
    bool IsLightPressure() const;
    
    // 长时间不动检测
    bool IsLongTimeNoMovement() const;
    uint32_t GetNoMovementDuration() const; // 返回不动持续时间(秒)
    
    // 压力检测触发音乐播放
    void TriggerMusicPlayback();
    void TriggerMusicPauseback();

private:
    AdcManager() = default;
    ~AdcManager() = default;
    AdcManager(const AdcManager&) = delete;
    AdcManager& operator=(const AdcManager&) = delete;
    
    void InitializeAdc();
    void CheckLongTimeNoMovement(int adc_value);
    static void AdcTask(void *pvParameters);
    
    bool initialized_ = false;
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    
    // 压感传感器数据
    static constexpr size_t kPressureAdcDataCount = 10;
    int pressure_adc_values_[kPressureAdcDataCount];
    size_t pressure_data_index_ = 0;
    int current_pressure_value_ = 0;
    
    // 长时间不动检测相关变量
    int last_stable_value_ = 0;
    uint32_t no_movement_start_time_ = 0;
    bool is_no_movement_detected_ = false;
    static constexpr int kMovementThreshold = 50; // ADC变化阈值
    static constexpr uint32_t kLongTimeThreshold = 30; // 长时间阈值(秒)
    
    // 任务句柄
    TaskHandle_t adc_task_handle_ = nullptr;
};

#endif // ADC_MANAGER_H
