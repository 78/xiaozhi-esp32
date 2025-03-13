#include "system_reset.h"

#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>

#define TAG "SystemReset"  // 定义日志标签

// SystemReset类的构造函数
SystemReset::SystemReset(gpio_num_t reset_nvs_pin, gpio_num_t reset_factory_pin) : reset_nvs_pin_(reset_nvs_pin), reset_factory_pin_(reset_factory_pin) {
    // 配置GPIO引脚为输入模式，用于检测按钮按下
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 禁用中断
    io_conf.mode = GPIO_MODE_INPUT;  // 设置为输入模式
    io_conf.pin_bit_mask = (1ULL << reset_nvs_pin_) | (1ULL << reset_factory_pin_);  // 配置引脚掩码
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 禁用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // 启用上拉电阻
    gpio_config(&io_conf);  // 应用GPIO配置
}

// 检查按钮状态的函数
void SystemReset::CheckButtons() {
    // 如果复位出厂设置按钮被按下
    if (gpio_get_level(reset_factory_pin_) == 0) {
        ESP_LOGI(TAG, "Button is pressed, reset to factory");  // 记录日志
        ResetNvsFlash();  // 重置NVS闪存
        ResetToFactory();  // 复位到出厂设置
    }

    // 如果重置NVS闪存按钮被按下
    if (gpio_get_level(reset_nvs_pin_) == 0) {
        ESP_LOGI(TAG, "Button is pressed, reset NVS flash");  // 记录日志
        ResetNvsFlash();  // 重置NVS闪存
    }
}

// 重置NVS闪存的函数
void SystemReset::ResetNvsFlash() {
    ESP_LOGI(TAG, "Resetting NVS flash");  // 记录日志
    esp_err_t ret = nvs_flash_erase();  // 擦除NVS闪存
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS flash");  // 如果失败，记录错误日志
    }
    ret = nvs_flash_init();  // 初始化NVS闪存
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash");  // 如果失败，记录错误日志
    }
}

// 复位到出厂设置的函数
void SystemReset::ResetToFactory() {
    ESP_LOGI(TAG, "Resetting to factory");  // 记录日志
    // 查找otadata分区
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to find otadata partition");  // 如果找不到分区，记录错误日志
        return;
    }
    esp_partition_erase_range(partition, 0, partition->size);  // 擦除otadata分区
    ESP_LOGI(TAG, "Erased otadata partition");  // 记录日志

    // 在3秒后重启设备
    RestartInSeconds(3);
}

// 在指定秒数后重启设备的函数
void SystemReset::RestartInSeconds(int seconds) {
    for (int i = seconds; i > 0; i--) {
        ESP_LOGI(TAG, "Resetting in %d seconds", i);  // 记录倒计时日志
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 延迟1秒
    }
    esp_restart();  // 重启设备
}