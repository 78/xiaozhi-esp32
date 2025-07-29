#include "ota.h"
#include "system_info.h"
#include "board.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_random.h>

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>

#define TAG "Ota"


Ota::Ota() {
    SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
}

Ota::~Ota() {
}

void Ota::SetCheckVersionUrl(std::string check_version_url) {
    check_version_url_ = check_version_url;
}

void Ota::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    // Check if there is a new firmware version available
    current_version_ = app_desc->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    if (check_version_url_.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    // 添加调试日志显示要请求的URL
    ESP_LOGI(TAG, "Checking OTA version from URL: %s", check_version_url_.c_str());

    auto http = board.CreateHttp();
    for (const auto& header : headers_) {
        http->SetHeader(header.first, header.second);
    }

    // 生成随机值
    std::string random_client_id = GenerateRandomUUID();
    std::string random_device_id = GenerateRandomMacAddress();

    http->SetHeader("Authorization", "Bearer oaibro-hugh-test");
    http->SetHeader("Ota-Version", "2");
    http->SetHeader("Device-Id", random_device_id.c_str());
    http->SetHeader("Client-Id", random_client_id.c_str());
    http->SetHeader("X-Client-Id", "3095dd17-a431-4a49-90e5-2207a31d327e");
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");

    // 添加调试日志显示请求头
    ESP_LOGI(TAG, "OTA请求头设置:");
    ESP_LOGI(TAG, "  Authorization: Bearer oaibro-hugh-test");
    ESP_LOGI(TAG, "  Device-Id: %s", random_device_id.c_str());
    ESP_LOGI(TAG, "  Client-Id: %s", random_client_id.c_str());
    ESP_LOGI(TAG, "  X-Client-Id: 3095dd17-a431-4a49-90e5-2207a31d327e");

    std::string post_data = board.GetJson();
    std::string method = post_data.length() > 0 ? "POST" : "GET";
    
    // 添加更详细的调试信息
    ESP_LOGI(TAG, "HTTP method: %s, Post data length: %d", method.c_str(), post_data.length());
    if (post_data.length() > 0) {
        ESP_LOGI(TAG, "Post data: %s", post_data.c_str());
    }
    
    if (!http->Open(method, check_version_url_, post_data)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection to URL: %s", check_version_url_.c_str());
        delete http;
        return false;
    }

    auto response = http->GetBody();
    // http->Close();
    // delete http;

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    has_activation_code_ = false;
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation != NULL) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (message != NULL) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (code != NULL) {
            activation_code_ = code->valuestring;
        }
        has_activation_code_ = true;
    }

    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt != NULL) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (item->type == cJSON_String) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            }
        }
        has_mqtt_config_ = true;
    }

    has_server_time_ = false;
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (server_time != NULL) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        
        if (timestamp != NULL) {
            // 设置系统时间
            struct timeval tv;
            double ts = timestamp->valuedouble;
            
            // 如果有时区偏移，计算本地时间
            if (timezone_offset != NULL) {
                ts += (timezone_offset->valueint * 60 * 1000); // 转换分钟为毫秒
            }
            
            tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    }

    has_websocket_config_ = false;
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket != NULL) {
        cJSON *url = cJSON_GetObjectItem(websocket, "url");
        cJSON *token = cJSON_GetObjectItem(websocket, "token");
        
        if (url != NULL && token != NULL) {
            websocket_url_ = url->valuestring;
            websocket_token_ = token->valuestring;
            has_websocket_config_ = true;
            ESP_LOGI(TAG, "Websocket config updated: %s", websocket_url_.c_str());
        }
    }

    // 如果设备未绑定（有activation code），则跳过固件检查
    if (has_activation_code_) {
        ESP_LOGI(TAG, "Device not activated yet, skipping firmware version check");
        has_new_version_ = false;  // 未绑定设备不需要升级
        cJSON_Delete(root);
        return true;
    }

    // 只有已绑定的设备才检查firmware字段
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware == NULL) {
        ESP_LOGE(TAG, "Failed to get firmware object for activated device");
        cJSON_Delete(root);
        return false;
    }

    cJSON *version = cJSON_GetObjectItem(firmware, "version");
    if (version == NULL) {
        ESP_LOGE(TAG, "Failed to get version object");
        cJSON_Delete(root);
        return false;
    }

    cJSON *url = cJSON_GetObjectItem(firmware, "url");
    if (url == NULL) {
        ESP_LOGE(TAG, "Failed to get url object");
        cJSON_Delete(root);
        return false;
    }

    firmware_version_ = version->valuestring;
    firmware_url_ = url->valuestring;
    cJSON_Delete(root);

    // Check if the version is newer
    has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
    if (has_new_version_) {
        ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
    } else {
        ESP_LOGI(TAG, "Current is the latest version");
    }
    return true;
}

void Ota::MarkCurrentVersionValid() {
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

void Ota::Upgrade(const std::string& firmware_url) {
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

    char buffer[512];
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
                std::string().swap(image_header);
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

void Ota::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;
    Upgrade(firmware_url_);
}

std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
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

std::string Ota::GenerateRandomUUID() {
    // 生成随机UUID格式：xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // 生成32个随机十六进制字符，按UUID格式分组
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        ss << std::setw(1) << (esp_random() & 0xF);
    }

    return ss.str();
}

std::string Ota::GenerateRandomMacAddress() {
    // 生成随机MAC地址格式：xx:xx:xx:xx:xx:xx
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 6; i++) {
        if (i > 0) {
            ss << ":";
        }
        ss << std::setw(2) << (esp_random() & 0xFF);
    }

    return ss.str();
}
