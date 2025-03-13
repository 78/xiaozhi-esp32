#include "system_info.h"

#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>

#define TAG "SystemInfo"

// 获取Flash大小
// 返回值: Flash的大小（以字节为单位），如果获取失败则返回0
size_t SystemInfo::GetFlashSize() {
    uint32_t flash_size;
    // 调用esp_flash_get_size获取Flash大小
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size");
        return 0;
    }
    return (size_t)flash_size;
}

// 获取最小空闲堆大小
// 返回值: 当前最小空闲堆大小（以字节为单位）
size_t SystemInfo::GetMinimumFreeHeapSize() {
    return esp_get_minimum_free_heap_size();
}

// 获取当前空闲堆大小
// 返回值: 当前空闲堆大小（以字节为单位）
size_t SystemInfo::GetFreeHeapSize() {
    return esp_get_free_heap_size();
}

// 获取MAC地址
// 返回值: 格式化为字符串的MAC地址（例如："aa:bb:cc:dd:ee:ff"）
std::string SystemInfo::GetMacAddress() {
    uint8_t mac[6];
    // 读取WiFi STA模式的MAC地址
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    // 将MAC地址格式化为字符串
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}

// 获取芯片型号名称
// 返回值: 芯片型号名称（例如："ESP32"）
std::string SystemInfo::GetChipModelName() {
    return std::string(CONFIG_IDF_TARGET);
}

// 打印实时任务统计信息
// 参数:
// - xTicksToWait: 等待时间（以Tick为单位），用于计算任务运行时间的间隔
// 返回值: ESP_OK表示成功，其他值表示失败
esp_err_t SystemInfo::PrintRealTimeStats(TickType_t xTicksToWait) {
    #define ARRAY_SIZE_OFFSET 5
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret;
    uint32_t total_elapsed_time;

    // 分配数组以存储当前任务状态
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // 获取当前任务状态
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    // 等待指定时间
    vTaskDelay(xTicksToWait);

    // 分配数组以存储延迟后的任务状态
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // 获取延迟后的任务状态
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    // 计算总运行时间（以运行统计时钟周期为单位）
    total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    // 打印表头
    printf("| Task | Run Time | Percentage\n");
    // 将start_array中的每个任务与end_array中的任务匹配
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                // 通过覆盖句柄标记任务已匹配
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        // 检查是否找到匹配的任务
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
            printf("| %-16s | %8lu | %4lu%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    // 打印未匹配的任务（已删除或新创建的任务）
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    // 公共返回路径
    free(start_array);
    free(end_array);
    return ret;
}