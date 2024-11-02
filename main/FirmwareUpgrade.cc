#include "FirmwareUpgrade.h"
#include "SystemInfo.h"
#include "Board.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_chip_info.h>

#include <vector>
#include <sstream>
#include <algorithm>

#define TAG "FirmwareUpgrade"


FirmwareUpgrade::FirmwareUpgrade() {
}

FirmwareUpgrade::~FirmwareUpgrade() {
}

void FirmwareUpgrade::SetCheckVersionUrl(std::string check_version_url) {
    check_version_url_ = check_version_url;
}

void FirmwareUpgrade::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void FirmwareUpgrade::CheckVersion() {
    std::string current_version = esp_app_get_description()->version;
    ESP_LOGI(TAG, "Current version: %s", current_version.c_str());

    if (check_version_url_.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return;
    }

    auto http = Board::GetInstance().CreateHttp();
    for (const auto& header : headers_) {
        http->SetHeader(header.first, header.second);
    }

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(GetPostData());
    http->Open("POST", check_version_url_);

    auto response = http->GetBody();
    http->Close();
    delete http;

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return;
    }
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware == NULL) {
        ESP_LOGE(TAG, "Failed to get firmware object");
        cJSON_Delete(root);
        return;
    }
    cJSON *version = cJSON_GetObjectItem(firmware, "version");
    if (version == NULL) {
        ESP_LOGE(TAG, "Failed to get version object");
        cJSON_Delete(root);
        return;
    }
    cJSON *url = cJSON_GetObjectItem(firmware, "url");
    if (url == NULL) {
        ESP_LOGE(TAG, "Failed to get url object");
        cJSON_Delete(root);
        return;
    }

    firmware_version_ = version->valuestring;
    firmware_url_ = url->valuestring;
    cJSON_Delete(root);

    // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
    has_new_version_ = IsNewVersionAvailable(current_version, firmware_version_);
    if (has_new_version_) {
        ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
    } else {
        ESP_LOGI(TAG, "Current is the latest version");
    }
}

void FirmwareUpgrade::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

void FirmwareUpgrade::Upgrade(const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);
    bool image_header_checked = false;
    std::string image_header;

    auto http = Board::GetInstance().CreateHttp();
    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return;
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        delete http;
        return;
    }

    char buffer[4096];
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            delete http;
            return;
        }

        // Calculate speed and progress every second
        recent_read += ret;
        total_read += ret;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %zu%% (%zu/%zu), Speed: %zuB/s", progress, total_read, content_length, recent_read);
            if (upgrade_callback_) {
                upgrade_callback_(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }

        if (ret == 0) {
            break;
        }


        if (!image_header_checked) {
            image_header.append(buffer, ret);
            if (image_header.size() >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, image_header.data() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                auto current_version = esp_app_get_description()->version;
                if (memcmp(new_app_info.version, current_version, sizeof(new_app_info.version)) == 0) {
                    ESP_LOGE(TAG, "Firmware version is the same, skipping upgrade");
                    delete http;
                    return;
                }

                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle)) {
                    esp_ota_abort(update_handle);
                    delete http;
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    return;
                }

                image_header_checked = true;
            }
        }
        auto err = esp_ota_write(update_handle, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            delete http;
            return;
        }
    }
    delete http;

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful, rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

void FirmwareUpgrade::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;
    Upgrade(firmware_url_);
}

std::vector<int> FirmwareUpgrade::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}

bool FirmwareUpgrade::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;
        } else if (newer[i] < current[i]) {
            return false;
        }
    }
    
    return newer.size() > current.size();
}

void FirmwareUpgrade::SetBoardJson(const std::string& board_json) {
    board_json_ = board_json;
}

std::string FirmwareUpgrade::GetPostData() {
    /* 
        {
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            }
        }
    */
    std::string json = "{";
    json += "\"flash_size\":" + std::to_string(SystemInfo::GetFlashSize()) + ",";
    json += "\"minimum_free_heap_size\":" + std::to_string(SystemInfo::GetMinimumFreeHeapSize()) + ",";
    json += "\"mac_address\":\"" + SystemInfo::GetMacAddress() + "\",";
    json += "\"chip_model_name\":\"" + SystemInfo::GetChipModelName() + "\",";
    json += "\"chip_info\":{";

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    json += "\"model\":" + std::to_string(chip_info.model) + ",";
    json += "\"cores\":" + std::to_string(chip_info.cores) + ",";
    json += "\"revision\":" + std::to_string(chip_info.revision) + ",";
    json += "\"features\":" + std::to_string(chip_info.features);
    json += "},";

    json += "\"application\":{";
    auto app_desc = esp_app_get_description();
    json += "\"name\":\"" + std::string(app_desc->project_name) + "\",";
    json += "\"version\":\"" + std::string(app_desc->version) + "\",";
    json += "\"compile_time\":\"" + std::string(app_desc->date) + "T" + std::string(app_desc->time) + "Z\",";
    json += "\"idf_version\":\"" + std::string(app_desc->idf_ver) + "\",";

    char sha256_str[65];
    for (int i = 0; i < 32; i++) {
        snprintf(sha256_str + i * 2, sizeof(sha256_str) - i * 2, "%02x", app_desc->app_elf_sha256[i]);
    }
    json += "\"elf_sha256\":\"" + std::string(sha256_str) + "\"";
    json += "},";

    json += "\"partition_table\": [";
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *partition = esp_partition_get(it);
        json += "{";
        json += "\"label\":\"" + std::string(partition->label) + "\",";
        json += "\"type\":" + std::to_string(partition->type) + ",";
        json += "\"subtype\":" + std::to_string(partition->subtype) + ",";
        json += "\"address\":" + std::to_string(partition->address) + ",";
        json += "\"size\":" + std::to_string(partition->size);
        json += "},";
        it = esp_partition_next(it);
    }
    json.pop_back(); // Remove the last comma
    json += "],";

    json += "\"ota\":{";
    auto ota_partition = esp_ota_get_running_partition();
    json += "\"label\":\"" + std::string(ota_partition->label) + "\"";
    json += "},";

    json += "\"board\":" + board_json_;

    // Close the JSON object
    json += "}";
    return json;
}
