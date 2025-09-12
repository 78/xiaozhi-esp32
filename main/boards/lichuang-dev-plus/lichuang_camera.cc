#include "lichuang_camera.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "LichuangDevPlusCamera"

LichuangDevPlusCamera::LichuangDevPlusCamera(const camera_config_t& config) : Esp32Camera(config) {
    // 等待摄像头初始化完成
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 获取底层的 sensor_t 对象，设置镜像和翻转
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        // 强制设置水平镜像，覆盖基类的设置
        s->set_hmirror(s, 1);
        ESP_LOGI(TAG, "Horizontal mirror forced to enabled");
        
        // 设置垂直翻转
        s->set_vflip(s, 1);
        ESP_LOGI(TAG, "Vertical flip enabled");
    } else {
        ESP_LOGE(TAG, "Failed to get camera sensor handle");
    }
}
