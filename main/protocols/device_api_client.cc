#include "device_api_client.h"
#include "agora_rtc_protocol.h"
#include "board.h"
#include "system_info.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_app_desc.h>
#include <cstring>

#define TAG "DeviceAPI"

// NVS namespace for device credentials
#define NVS_NAMESPACE "agora_dev"
#define NVS_KEY_DEVICE_TOKEN "dev_token"

DeviceApiClient::DeviceApiClient() {
    // Generate device_id from MAC: AG-XXXXXXXXXXXX (last 6 hex chars uppercase)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id_buf[20];
    snprintf(id_buf, sizeof(id_buf), "AG-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    device_id_ = id_buf;

    // Load persisted device_token from NVS
    LoadDeviceToken();
    ESP_LOGI(TAG, "Device ID: %s, has_token: %s", device_id_.c_str(),
             device_token_.empty() ? "no" : "yes");
}

bool DeviceApiClient::HasDeviceToken() const {
    return !device_token_.empty();
}

void DeviceApiClient::LoadDeviceToken() {
    Settings settings(NVS_NAMESPACE, false);
    device_token_ = settings.GetString(NVS_KEY_DEVICE_TOKEN);
}

void DeviceApiClient::SaveDeviceToken(const std::string& token) {
    device_token_ = token;
    Settings settings(NVS_NAMESPACE, true);
    settings.SetString(NVS_KEY_DEVICE_TOKEN, token);
    ESP_LOGI(TAG, "Device token saved to NVS");
}

void DeviceApiClient::ClearCredentials() {
    device_token_.clear();
    pair_token_.clear();
    Settings settings(NVS_NAMESPACE, true);
    settings.EraseKey(NVS_KEY_DEVICE_TOKEN);
    ESP_LOGI(TAG, "Credentials cleared");
}

std::string DeviceApiClient::BuildUrl(const std::string& path) const {
    std::string base = CONFIG_DEVICE_API_SERVER_URL;
    // Remove trailing slash if present
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + path;
}

DeviceApiError DeviceApiClient::ParseErrorResponse(int status_code, const std::string& body) {
    if (status_code == 401) {
        // Check if it's token revoked
        auto root = cJSON_Parse(body.c_str());
        if (root) {
            auto error = cJSON_GetObjectItem(root, "error");
            if (error) {
                auto code = cJSON_GetObjectItem(error, "code");
                if (code && cJSON_IsString(code)) {
                    if (strcmp(code->valuestring, "DEVICE_TOKEN_REVOKED") == 0) {
                        cJSON_Delete(root);
                        return DeviceApiError::kTokenRevoked;
                    }
                }
            }
            cJSON_Delete(root);
        }
        return DeviceApiError::kUnauthenticated;
    } else if (status_code == 403) {
        return DeviceApiError::kForbidden;
    } else if (status_code == 409) {
        return DeviceApiError::kNotBound;
    } else if (status_code == 429) {
        return DeviceApiError::kRateLimited;
    } else if (status_code >= 500) {
        return DeviceApiError::kServerError;
    }
    return DeviceApiError::kNetworkError;
}

std::string DeviceApiClient::RequestPairCode() {
    last_error_ = DeviceApiError::kNone;

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("Content-Type", "application/json");
    http->SetTimeout(10000);

    // Build request body
    auto app_desc = esp_app_get_description();
    cJSON* body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "device_id", device_id_.c_str());
    cJSON_AddStringToObject(body, "firmware_version", app_desc->version);
    cJSON_AddStringToObject(body, "hardware_model", BOARD_NAME);
    char* body_str = cJSON_PrintUnformatted(body);
    ESP_LOGI(TAG, "POST /devices/pair-codes body=%s", body_str);
    http->SetContent(std::string(body_str));
    cJSON_free(body_str);
    cJSON_Delete(body);

    std::string url = BuildUrl("/devices/pair-codes");

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        last_error_ = DeviceApiError::kNetworkError;
        return "";
    }

    int status = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "Response: status=%d, body=%s", status, response.c_str());

    if (status != 201 && status != 200) {
        ESP_LOGE(TAG, "Pair code request failed: %d", status);
        last_error_ = ParseErrorResponse(status, response);
        return "";
    }

    // Parse response
    auto root = cJSON_Parse(response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse pair code response");
        last_error_ = DeviceApiError::kServerError;
        return "";
    }

    auto data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        ESP_LOGE(TAG, "No 'data' in pair code response");
        cJSON_Delete(root);
        last_error_ = DeviceApiError::kServerError;
        return "";
    }

    auto code_item = cJSON_GetObjectItem(data, "code");
    auto pair_token_item = cJSON_GetObjectItem(data, "pair_token");

    std::string code;
    if (cJSON_IsString(code_item) && cJSON_IsString(pair_token_item)) {
        code = code_item->valuestring;
        pair_token_ = pair_token_item->valuestring;
        ESP_LOGI(TAG, "Got pair code (len=%d)", (int)code.size());
    } else {
        ESP_LOGE(TAG, "Invalid pair code response format");
        last_error_ = DeviceApiError::kServerError;
    }

    cJSON_Delete(root);
    return code;
}

DeviceApiClient::PollResult DeviceApiClient::PollBindingStatus(int& poll_after_seconds) {
    last_error_ = DeviceApiError::kNone;
    poll_after_seconds = 3;

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("Content-Type", "application/json");
    http->SetTimeout(10000);

    // Use device_token if available, otherwise pair_token
    if (!device_token_.empty()) {
        http->SetHeader("Authorization", std::string("Device ") + device_token_);
        ESP_LOGI(TAG, "GET binding-status auth=Device token_len=%d", (int)device_token_.size());
    } else if (!pair_token_.empty()) {
        http->SetHeader("Authorization", std::string("Pair ") + pair_token_);
        ESP_LOGI(TAG, "GET binding-status auth=Pair token_len=%d", (int)pair_token_.size());
    } else {
        ESP_LOGE(TAG, "No token available for binding status poll");
        last_error_ = DeviceApiError::kUnauthenticated;
        return PollResult::kError;
    }

    std::string url = BuildUrl("/devices/" + device_id_ + "/binding-status");

    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for binding status");
        last_error_ = DeviceApiError::kNetworkError;
        return PollResult::kError;
    }

    int status = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "binding-status response: status=%d, body=%s", status, response.c_str());

    if (status == 401 || status == 403) {
        last_error_ = ParseErrorResponse(status, response);
        return PollResult::kError;
    }

    if (status != 200) {
        ESP_LOGE(TAG, "Binding status poll failed: %d", status);
        last_error_ = ParseErrorResponse(status, response);
        return PollResult::kError;
    }

    // Parse response
    auto root = cJSON_Parse(response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse binding status response");
        last_error_ = DeviceApiError::kServerError;
        return PollResult::kError;
    }

    auto data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        cJSON_Delete(root);
        last_error_ = DeviceApiError::kServerError;
        return PollResult::kError;
    }

    auto status_item = cJSON_GetObjectItem(data, "status");
    auto poll_item = cJSON_GetObjectItem(data, "poll_after_seconds");

    if (cJSON_IsNumber(poll_item)) {
        poll_after_seconds = poll_item->valueint;
    }

    PollResult result = PollResult::kError;

    if (cJSON_IsString(status_item)) {
        const char* s = status_item->valuestring;
        if (strcmp(s, "pending") == 0) {
            result = PollResult::kPending;
        } else if (strcmp(s, "bound") == 0) {
            // Check if device_token is present (first time bound)
            auto token_item = cJSON_GetObjectItem(data, "device_token");
            if (cJSON_IsString(token_item) && strlen(token_item->valuestring) > 0) {
                SaveDeviceToken(token_item->valuestring);
                // Clear pair_token from memory as per spec:
                // "拿到 device_token 后，优先用 device_token 取代 pair_token"
                pair_token_.clear();
            }
            result = PollResult::kBound;
        } else if (strcmp(s, "expired") == 0) {
            result = PollResult::kExpired;
        } else if (strcmp(s, "unbound") == 0) {
            // Device has been unbound by user on Web side
            // Clear local credentials and signal caller to re-pair
            ClearCredentials();
            result = PollResult::kUnbound;
        } else if (strcmp(s, "failed") == 0) {
            last_error_ = DeviceApiError::kServerError;
            result = PollResult::kError;
        }
    }

    cJSON_Delete(root);
    return result;
}

bool DeviceApiClient::StartConversation(ConversationInfo& info) {
    last_error_ = DeviceApiError::kNone;

    if (device_token_.empty()) {
        ESP_LOGE(TAG, "No device_token, cannot start conversation");
        last_error_ = DeviceApiError::kUnauthenticated;
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("Content-Type", "application/json");
    http->SetHeader("Authorization", std::string("Device ") + device_token_);
    http->SetTimeout(15000);

    // Build request body
    cJSON* body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "trigger", "button");

    cJSON* audio = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio, "p_time", 60);
    cJSON_AddStringToObject(audio, "codec", "G722");
    cJSON_AddNumberToObject(audio, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio, "bit_depth", 16);
    cJSON_AddNumberToObject(audio, "channels", 1);
    cJSON_AddItemToObject(body, "audio", audio);

    cJSON* features = cJSON_CreateObject();
    cJSON_AddBoolToObject(features, "cloud_aec", AGORA_CLOUD_AEC);
    cJSON_AddBoolToObject(features, "ai_qos", AGORA_AI_QOS);
    cJSON_AddNumberToObject(features, "fast_send_multiplier", 2);
    cJSON_AddItemToObject(body, "features", features);

    auto app_desc = esp_app_get_description();
    cJSON_AddStringToObject(body, "firmware_version", app_desc->version);

    char* body_str = cJSON_PrintUnformatted(body);
    ESP_LOGI(TAG, "POST conversations/start body=%s", body_str);
    http->SetContent(std::string(body_str));
    cJSON_free(body_str);
    cJSON_Delete(body);

    std::string url = BuildUrl("/devices/" + device_id_ + "/conversations/start");

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for start conversation");
        last_error_ = DeviceApiError::kNetworkError;
        return false;
    }

    int status = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "conversations/start response: status=%d, body=%s", status, response.c_str());

    if (status == 401 || status == 409) {
        last_error_ = ParseErrorResponse(status, response);
        // Token revoked or not bound - need to re-pair
        if (last_error_ == DeviceApiError::kTokenRevoked ||
            last_error_ == DeviceApiError::kNotBound ||
            last_error_ == DeviceApiError::kUnauthenticated) {
            ClearCredentials();
        }
        return false;
    }

    if (status == 410) {
        // CONVERSATION_EXPIRED: RTC token expired, caller should retry
        last_error_ = DeviceApiError::kExpired;
        return false;
    }

    if (status >= 500) {
        // 502 RTC_TOKEN_ISSUE_FAILED / CONVERSATION_START_FAILED: server error, backoff retry
        ESP_LOGW(TAG, "Start conversation server error: %d, retrying...", status);
        last_error_ = DeviceApiError::kServerError;
        return false;
    }

    if (status != 201 && status != 200) {
        ESP_LOGE(TAG, "Start conversation failed: %d", status);
        last_error_ = ParseErrorResponse(status, response);
        return false;
    }

    // Parse response
    auto root = cJSON_Parse(response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse start conversation response");
        last_error_ = DeviceApiError::kServerError;
        return false;
    }

    auto data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        cJSON_Delete(root);
        last_error_ = DeviceApiError::kServerError;
        return false;
    }

    // Extract conversation info
    auto conv_id = cJSON_GetObjectItem(data, "conversation_id");
    auto agent_id = cJSON_GetObjectItem(data, "agent_id");
    auto agent_uid = cJSON_GetObjectItem(data, "agent_uid");

    if (cJSON_IsString(conv_id)) {
        info.conversation_id = conv_id->valuestring;
    }
    if (cJSON_IsString(agent_id)) {
        info.agent_id = agent_id->valuestring;
    }

    // Extract RTC config
    auto rtc = cJSON_GetObjectItem(data, "rtc");
    if (rtc) {
        auto app_id_item = cJSON_GetObjectItem(rtc, "app_id");
        auto channel_item = cJSON_GetObjectItem(rtc, "channel");
        auto token_item = cJSON_GetObjectItem(rtc, "token");
        auto uid_item = cJSON_GetObjectItem(rtc, "uid");

        if (cJSON_IsString(app_id_item)) info.rtc.app_id = app_id_item->valuestring;
        if (cJSON_IsString(channel_item)) info.rtc.channel = channel_item->valuestring;
        if (cJSON_IsString(token_item)) info.rtc.token = token_item->valuestring;
        if (cJSON_IsString(uid_item)) {
            info.rtc.uid = uid_item->valuestring;
        } else if (cJSON_IsNumber(uid_item)) {
            info.rtc.uid = std::to_string(uid_item->valueint);
        }
    }
    if (cJSON_IsString(agent_uid)) {
        info.rtc.agent_uid = agent_uid->valuestring;
    } else if (cJSON_IsNumber(agent_uid)) {
        info.rtc.agent_uid = std::to_string(agent_uid->valueint);
    }

    ESP_LOGI(TAG, "Conversation started: id=%s, channel=%s, uid=%s",
             info.conversation_id.c_str(), info.rtc.channel.c_str(),
             info.rtc.uid.c_str());

    cJSON_Delete(root);
    return true;
}

bool DeviceApiClient::StopConversation(const std::string& conversation_id, const std::string& reason) {
    last_error_ = DeviceApiError::kNone;

    if (device_token_.empty() || conversation_id.empty()) {
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("Content-Type", "application/json");
    http->SetHeader("Authorization", std::string("Device ") + device_token_);
    http->SetTimeout(10000);

    cJSON* body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "conversation_id", conversation_id.c_str());
    cJSON_AddStringToObject(body, "reason", reason.c_str());
    char* body_str = cJSON_PrintUnformatted(body);
    ESP_LOGI(TAG, "POST conversations/stop body=%s", body_str);
    http->SetContent(std::string(body_str));
    cJSON_free(body_str);
    cJSON_Delete(body);

    std::string url = BuildUrl("/devices/" + device_id_ + "/conversations/stop");

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for stop conversation");
        last_error_ = DeviceApiError::kNetworkError;
        return false;
    }

    int status = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "conversations/stop response: status=%d, body=%s", status, response.c_str());

    if (status == 200 || status == 201) {
        ESP_LOGI(TAG, "Conversation stopped successfully");
        return true;
    }

    if (status == 401 || status == 409) {
        last_error_ = ParseErrorResponse(status, response);
        if (last_error_ == DeviceApiError::kTokenRevoked ||
            last_error_ == DeviceApiError::kNotBound ||
            last_error_ == DeviceApiError::kUnauthenticated) {
            ClearCredentials();
        }
        // Still return true - the conversation is effectively over
        return true;
    }

    ESP_LOGW(TAG, "Stop conversation returned status: %d", status);
    return true; // Idempotent - treat as success even on error
}
