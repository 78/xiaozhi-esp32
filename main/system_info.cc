#include "system_info.h"

#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <nvs.h>


#define TAG "SystemInfo"

size_t SystemInfo::GetFlashSize() {
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size");
        return 0;
    }
    return (size_t)flash_size;
}

size_t SystemInfo::GetMinimumFreeHeapSize() {
    return esp_get_minimum_free_heap_size();
}

size_t SystemInfo::GetFreeHeapSize() {
    return esp_get_free_heap_size();
}

std::string SystemInfo::GetMacAddress() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}

std::string SystemInfo::GetClientId() {
    // 尝试从NVS读取存储的Client-Id
    std::string client_id;
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("websocket", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = 0;
        err = nvs_get_str(nvs_handle, "client_id", NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            char* client_id_buf = (char*)malloc(required_size);
            if (client_id_buf != NULL) {
                err = nvs_get_str(nvs_handle, "client_id", client_id_buf, &required_size);
                if (err == ESP_OK) {
                    client_id = client_id_buf;
                    ESP_LOGI(TAG, "Client-Id loaded from NVS: %s", client_id.c_str());
                }
                free(client_id_buf);
            }
        }
        
        // 只有在NVS中没有Client-Id时，才尝试从配置中获取并存储
        if (client_id.empty()) {
            ESP_LOGI(TAG, "No Client-Id found in NVS, checking configuration...");
#ifdef CONFIG_WEBSOCKET_CLIENT_ID
            std::string config_client_id = CONFIG_WEBSOCKET_CLIENT_ID;
            if (!config_client_id.empty()) {
                ESP_LOGI(TAG, "Found Client-Id in configuration: %s", config_client_id.c_str());
                err = nvs_set_str(nvs_handle, "client_id", config_client_id.c_str());
                if (err == ESP_OK) {
                    err = nvs_commit(nvs_handle);
                    if (err == ESP_OK) {
                        client_id = config_client_id;
                        ESP_LOGI(TAG, "Client-Id stored to NVS from configuration: %s", client_id.c_str());
                    } else {
                        ESP_LOGE(TAG, "Failed to commit client_id to NVS: %s", esp_err_to_name(err));
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to set client_id in NVS: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGW(TAG, "CONFIG_WEBSOCKET_CLIENT_ID is empty");
            }
#else
            ESP_LOGW(TAG, "CONFIG_WEBSOCKET_CLIENT_ID not defined in this firmware");
#endif
        }
        
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for client_id: %s", esp_err_to_name(err));
    }
    
    if (client_id.empty()) {
        ESP_LOGW(TAG, "No Client-Id available, will use Board UUID as fallback");
    }
    
    return client_id;
}

std::string SystemInfo::GetChipModelName() {
    return std::string(CONFIG_IDF_TARGET);
}

esp_err_t SystemInfo::PrintRealTimeStats(TickType_t xTicksToWait) {
    #define ARRAY_SIZE_OFFSET 5
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret;
    uint32_t total_elapsed_time;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
            printf("| %-16s | %8lu | %4lu%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
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

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

