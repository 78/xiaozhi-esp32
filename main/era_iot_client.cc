#include "era_iot_client.h"
#include "board.h"
#include <esp_log.h>
#include <cJSON.h>

#define TAG "EraIotClient"

// static const char *ERA_AUTH_TOKEN = "Token b027d04220a93e1fc1a91be8fcde195ab25bdcd6";
// static const char *ERA_BASE_URL = "https://backend.eoh.io";
// static const char *ERA_CONFIG_ID = "150632";
// static const char *ERA_ACTION_ON_KEY = "ced48cf9-b159-4f2e-87de-44aaa6ea08c0";
// static const char *ERA_ACTION_OFF_KEY = "6bd3f760-453d-4a01-89c4-ee1f4559fcb8";

static const char *ERA_AUTH_TOKEN = "your_token_here";
static const char *ERA_BASE_URL = "your_base_url_here";
static const char *ERA_CONFIG_ID = "your_config_id_here";
static const char *ERA_ACTION_ON_KEY = "your_action_on_key_here";
static const char *ERA_ACTION_OFF_KEY = "your_action_off_key_here";
EraIotClient::EraIotClient() : initialized_(false)
{
}

EraIotClient::~EraIotClient()
{
}

void EraIotClient::Initialize(const std::string &auth_token, const std::string &base_url)
{
    auth_token_ = auth_token.empty() ? ERA_AUTH_TOKEN : auth_token;
    base_url_ = base_url.empty() ? ERA_BASE_URL : base_url;
    config_id_ = ERA_CONFIG_ID;
    action_on_key_ = ERA_ACTION_ON_KEY;
    action_off_key_ = ERA_ACTION_OFF_KEY;
    initialized_ = true;

    ESP_LOGI(TAG, "E-Ra IoT Client initialized with base URL: %s", base_url_.c_str());
}

std::string EraIotClient::GetCurrentValue(const std::string &config_id)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return "";
    }

    std::string endpoint = "/api/chip_manager/configs/" + config_id + "/current_value/";
    std::string response = MakeRequest("GET", endpoint);

    if (response.empty())
    {
        ESP_LOGE(TAG, "Failed to get current value for config %s", config_id.c_str());
        return "";
    }

    ESP_LOGI(TAG, "Raw response for config %s: %s", config_id.c_str(), response.c_str());

    // Parse JSON response to extract current_value_only
    cJSON *json = cJSON_Parse(response.c_str());
    if (json == nullptr)
    {
        ESP_LOGE(TAG, "Failed to parse response JSON");
        return "";
    }

    auto current_value = cJSON_GetObjectItem(json, "current_value_only");
    std::string result;
    if (cJSON_IsNumber(current_value))
    {
        result = std::to_string(current_value->valueint);
    }
    else if (cJSON_IsString(current_value))
    {
        result = current_value->valuestring;
    }
    else if (cJSON_IsBool(current_value))
    {
        result = current_value->valueint ? "true" : "false";
    }

    cJSON_Delete(json);
    ESP_LOGI(TAG, "Got current value for config %s: %s", config_id.c_str(), result.c_str());
    return result;
}

bool EraIotClient::TriggerAction(const std::string &action_key)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return false;
    }

    std::string endpoint = "/api/chip_manager/trigger_action/";

    // Determine value based on action key (ON=1, OFF=0)
    int value = (action_key == action_on_key_) ? 1 : 0;

    // Create JSON payload according to E-Ra API format
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", action_key.c_str());

    // Add source
    cJSON_AddStringToObject(json, "source", "internet");

    char *json_string = cJSON_Print(json);
    std::string payload(json_string);
    free(json_string);
    cJSON_Delete(json);

    std::string response = MakeRequest("POST", endpoint, payload);
    bool success = !response.empty();

    if (success)
    {
        ESP_LOGI(TAG, "Successfully triggered action: %s with value: %d", action_key.c_str(), value);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to trigger action: %s", action_key.c_str());
    }

    return success;
}

bool EraIotClient::TurnDeviceOn()
{
    ESP_LOGI(TAG, "Turning device ON");
    return TriggerAction(action_on_key_);
}

bool EraIotClient::TurnDeviceOff()
{
    ESP_LOGI(TAG, "Turning device OFF");
    return TriggerAction(action_off_key_);
}

std::string EraIotClient::MakeRequest(const std::string &method, const std::string &endpoint, const std::string &payload)
{
    auto &board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (!network)
    {
        ESP_LOGE(TAG, "Network not available");
        return "";
    }

    auto http = network->CreateHttp(10); // 10 second timeout
    if (!http)
    {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return "";
    }

    std::string url = base_url_ + endpoint;

    // Set headers
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Authorization", auth_token_);
    http->SetHeader("User-Agent", "XiaoZhi-ESP32/1.0");
    http->SetHeader("Cache-Control", "no-cache");

    if (method == "POST")
    {
        http->SetHeader("Content-Type", "application/json");
    }

    ESP_LOGI(TAG, "Making %s request to: %s", method.c_str(), url.c_str());

    if (!http->Open(method, url))
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection to: %s", url.c_str());
        return "";
    }

    // Send payload for POST requests
    if (method == "POST" && !payload.empty())
    {
        ESP_LOGI(TAG, "Sending payload: %s", payload.c_str());
        http->Write(payload.c_str(), payload.size());
    }

    // Complete the request
    http->Write("", 0);

    int status_code = http->GetStatusCode();
    if (status_code != 200)
    {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
        // Try to read error response for debugging
        std::string error_response = http->ReadAll();
        ESP_LOGE(TAG, "Error response: %s", error_response.c_str());
        http->Close();
        return "";
    }

    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGD(TAG, "Response: %s", response.c_str());
    return response;
}