#include "era_iot_client.h"
#include "board.h"
#include <esp_log.h>
#include <cJSON.h>

#define TAG "EraIotClient"

static const char *ERA_AUTH_TOKEN = "Token 7da2578ede0a67dfbe13366428e01862cf64e2b5";
static const char *ERA_BASE_URL = "https://backend.eoh.io";

// Switch 1
static const char *ERA_SWITCH1_CONFIG_ID = "154532";
static const char *ERA_SWITCH1_ACTION_ON = "976d5ef7-803c-4950-a62d-cea9d9666a6b";
static const char *ERA_SWITCH1_ACTION_OFF = "6f2b7d2f-0ad2-491e-9e6a-f2364958cbb9";

// Switch 2
static const char *ERA_SWITCH2_CONFIG_ID = "154533";
static const char *ERA_SWITCH2_ACTION_ON = "b9364f39-51dd-41e8-89cd-c5a87a034330";
static const char *ERA_SWITCH2_ACTION_OFF = "ecff2b3a-36be-4762-bcae-cdaaf44b1e0f";

// Switch 3
static const char *ERA_SWITCH3_CONFIG_ID = "154534";
static const char *ERA_SWITCH3_ACTION_ON = "ab0d8064-c72c-4770-931f-a6f54e48c50a";
static const char *ERA_SWITCH3_ACTION_OFF = "67777a1d-a4f0-4f0c-8ef6-2f5be637d209";

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

bool EraIotClient::TriggerAction(const std::string &action_key, int value)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return false;
    }

    std::string endpoint = "/api/chip_manager/trigger_action/";

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

std::string EraIotClient::GetSwitchStatus(int index)
{
    const char *config_id = nullptr;
    switch (index)
    {
    case 1:
        config_id = ERA_SWITCH1_CONFIG_ID;
        break;
    case 2:
        config_id = ERA_SWITCH2_CONFIG_ID;
        break;
    case 3:
        config_id = ERA_SWITCH3_CONFIG_ID;
        break;
    default:
        ESP_LOGE(TAG, "Invalid switch index: %d", index);
        return "";
    }
    return GetCurrentValue(config_id);
}

bool EraIotClient::TurnSwitchOn(int index)
{
    const char *action_key = nullptr;
    switch (index)
    {
    case 1:
        action_key = ERA_SWITCH1_ACTION_ON;
        break;
    case 2:
        action_key = ERA_SWITCH2_ACTION_ON;
        break;
    case 3:
        action_key = ERA_SWITCH3_ACTION_ON;
        break;
    default:
        ESP_LOGE(TAG, "Invalid switch index: %d", index);
        return false;
    }
    return TriggerAction(action_key, 1);
}

bool EraIotClient::TurnSwitchOff(int index)
{
    const char *action_key = nullptr;
    switch (index)
    {
    case 1:
        action_key = ERA_SWITCH1_ACTION_OFF;
        break;
    case 2:
        action_key = ERA_SWITCH2_ACTION_OFF;
        break;
    case 3:
        action_key = ERA_SWITCH3_ACTION_OFF;
        break;
    default:
        ESP_LOGE(TAG, "Invalid switch index: %d", index);
        return false;
    }
    return TriggerAction(action_key, 0);
}