#ifndef SSCMA_CAMERA_H
#define SSCMA_CAMERA_H

#include <cstdint>
#include <lvgl.h>
#include <thread>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_jpeg_dec.h>
#include <mbedtls/base64.h>

#include "sscma_client.h"
#include "camera.h"

struct SscmaData {
    uint8_t* img;
    size_t len;
};
struct JpegData {
    uint8_t* buf;
    size_t len;
};

class SscmaCamera : public Camera {
private:
    lv_img_dsc_t preview_image_;
    std::string explain_url_;
    std::string explain_token_;
    sscma_client_io_handle_t sscma_client_io_handle_;
    sscma_client_handle_t sscma_client_handle_;
    QueueHandle_t sscma_data_queue_;
    JpegData jpeg_data_;
    jpeg_dec_handle_t jpeg_dec_;
    jpeg_dec_io_t *jpeg_io_;
    jpeg_dec_header_info_t *jpeg_out_;
    // 检测状态机
    enum DetectionState {
        IDLE,           // 空闲状态
        VALIDATING,     // 验证中（连续检测3秒）
        COOLDOWN        // 冷却期（等待重新检测）
    };
    
    DetectionState detection_state = IDLE;
    int64_t state_start_time = 0;
    bool need_start_cooldown = false; // 是否需要开始冷却期
    int64_t last_detected_time = 0; // 验证期间最后一次检测到物体的时间
    
    // 缓存检测配置参数以避免频繁NVS访问
    int cached_target_type = 0;

    int cached_detect_threshold = 75;
    int cached_detect_duration_sec = 2; // 检测持续时间2秒，确认人员持续存在
    int cached_detect_invoke_interval_sec = 8; // 默认15秒冷却期，避免频繁开始会话
    int cached_detect_debounce_sec = 1; // 验证期间人员离开的去抖动时间1秒

    int64_t cached_detect_duration_us = cached_detect_duration_sec * 1000000LL; // 3秒 = 3,000,000微秒
    int64_t cached_detect_invoke_interval_us = cached_detect_invoke_interval_sec * 1000000LL; // 1秒 = 1,000,000微秒
    const int64_t cached_detect_debounce_us = cached_detect_debounce_sec * 1000000LL; // 去抖动时间
    int64_t last_config_load_tm = 0;
public:
    SscmaCamera(esp_io_expander_handle_t io_exp_handle);
    ~SscmaCamera();

    virtual void SetExplainUrl(const std::string& url, const std::string& token);
    virtual bool Capture();
    // 翻转控制函数
    virtual bool SetHMirror(bool enabled) override;
    virtual bool SetVFlip(bool enabled) override;
    virtual std::string Explain(const std::string& question);
};

#endif // ESP32_CAMERA_H
