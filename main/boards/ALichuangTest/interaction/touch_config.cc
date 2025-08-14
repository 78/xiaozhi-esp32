#include "touch_config.h"
#include <esp_log.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "TouchConfig"

bool TouchConfigLoader::LoadFromFile(const char* filepath, TouchDetectionConfig& config) {
    // 使用C标准库读取文件
    FILE* file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGW(TAG, "Cannot open config file: %s", filepath);
        return false;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 65536) {  // 限制最大64KB
        ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
        fclose(file);
        return false;
    }
    
    // 分配缓冲区并读取文件
    char* json_str = (char*)malloc(file_size + 1);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON");
        fclose(file);
        return false;
    }
    
    size_t read_size = fread(json_str, 1, file_size, file);
    json_str[read_size] = '\0';
    fclose(file);
    
    // 解析JSON
    bool result = ParseFromJson(json_str, config);
    
    // 释放缓冲区
    free(json_str);
    
    return result;
}

TouchDetectionConfig TouchConfigLoader::LoadDefaults() {
    // 返回默认配置
    TouchDetectionConfig config;
    ESP_LOGI(TAG, "Using default touch detection parameters");
    ESP_LOGI(TAG, "  tap_max_duration_ms: %ld", config.tap_max_duration_ms);
    ESP_LOGI(TAG, "  hold_min_duration_ms: %ld", config.hold_min_duration_ms);
    ESP_LOGI(TAG, "  debounce_time_ms: %ld", config.debounce_time_ms);
    ESP_LOGI(TAG, "  touch_threshold_ratio: %.1f", config.touch_threshold_ratio);
    return config;
}

bool TouchConfigLoader::ParseFromJson(const char* json_str, TouchDetectionConfig& config) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }
    
    bool success = false;
    
    // 查找 touch_detection_parameters 节点
    cJSON* touch_params = cJSON_GetObjectItem(root, "touch_detection_parameters");
    if (touch_params) {
        // 读取各个参数
        cJSON* item = nullptr;
        
        item = cJSON_GetObjectItem(touch_params, "tap_max_duration_ms");
        if (item && cJSON_IsNumber(item)) {
            config.tap_max_duration_ms = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "hold_min_duration_ms");
        if (item && cJSON_IsNumber(item)) {
            config.hold_min_duration_ms = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "cradled_min_duration_ms");
        if (item && cJSON_IsNumber(item)) {
            config.cradled_min_duration_ms = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "tickled_window_ms");
        if (item && cJSON_IsNumber(item)) {
            config.tickled_window_ms = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "tickled_min_touches");
        if (item && cJSON_IsNumber(item)) {
            config.tickled_min_touches = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "debounce_time_ms");
        if (item && cJSON_IsNumber(item)) {
            config.debounce_time_ms = item->valueint;
        }
        
        item = cJSON_GetObjectItem(touch_params, "touch_threshold_ratio");
        if (item && cJSON_IsNumber(item)) {
            config.touch_threshold_ratio = item->valuedouble;
        }
        
        success = true;
        
        ESP_LOGI(TAG, "Loaded touch detection parameters from JSON:");
        ESP_LOGI(TAG, "  tap_max_duration_ms: %ld", config.tap_max_duration_ms);
        ESP_LOGI(TAG, "  hold_min_duration_ms: %ld", config.hold_min_duration_ms);
        ESP_LOGI(TAG, "  cradled_min_duration_ms: %ld", config.cradled_min_duration_ms);
        ESP_LOGI(TAG, "  tickled_window_ms: %ld", config.tickled_window_ms);
        ESP_LOGI(TAG, "  tickled_min_touches: %ld", config.tickled_min_touches);
        ESP_LOGI(TAG, "  debounce_time_ms: %ld", config.debounce_time_ms);
        ESP_LOGI(TAG, "  touch_threshold_ratio: %.1f", config.touch_threshold_ratio);
    } else {
        ESP_LOGW(TAG, "No touch_detection_parameters found in JSON");
    }
    
    cJSON_Delete(root);
    return success;
}