#include "config_web.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <cJSON.h>

#include <esp_app_desc.h>
#include <ota_url.h>
#include <cstring>
#include <string>
#include "application.h"
#include "config.h"
#include "settings.h"
#if CONFIG_SOC_WIFI_SUPPORTED
#include <esp_wifi.h>
#endif

namespace {
constexpr char kTag[] = "AliyunConfigWeb";
httpd_handle_t server = nullptr;
bool ap_mode = false;
std::function<void()> wifi_action;
extern const char config_html_start[] asm("_binary_config_html_start");
extern const char config_html_end[] asm("_binary_config_html_end");

esp_err_t Reply(httpd_req_t* request, const char* status, const char* body) {
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "http://192.168.4.1");
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, body);
}

esp_err_t GetPage(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, config_html_start, config_html_end - config_html_start);
}

esp_err_t GetConfig(httpd_req_t* request) {
    auto config = LoadAliyunRealtimeConfig();
    cJSON* root = cJSON_CreateObject();
    if (!root)
        return Reply(request, "500 Internal Server Error", "{}");
    cJSON_AddBoolToObject(root, "provisioning", ap_mode);
    cJSON_AddStringToObject(root, "board", BOARD_NAME);
    cJSON_AddStringToObject(root, "firmware", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "device_id", Board::GetInstance().GetUuid().c_str());
    Settings audio("audio");
    cJSON_AddNumberToObject(root, "volume", audio.GetInt("output_volume", 70));
#if CONFIG_SOC_WIFI_SUPPORTED
    wifi_ap_record_t ap = {};
    const bool connected = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    cJSON_AddBoolToObject(root, "wifi_connected", connected);
    cJSON_AddStringToObject(root, "ssid", connected ? reinterpret_cast<const char*>(ap.ssid) : "");
#endif
    Settings wifi("wifi");
    cJSON_AddStringToObject(root, "xiaozhi_ota_url", wifi.GetString("ota_url").c_str());
    cJSON_AddStringToObject(
        root, "device_state",
        DeviceStateMachine::GetStateName(Application::GetInstance().GetDeviceState()));
    cJSON_AddStringToObject(
        root, "mode",
        Application::GetInstance().GetListeningMode() == kListeningModeManualStop ? "manual"
                                                                                  : "auto");
    cJSON_AddStringToObject(root, "websocket_url", config.websocket_url.c_str());
    cJSON_AddBoolToObject(root, "debug_input_pcm", config.debug_input_pcm);
    cJSON_AddBoolToObject(root, "selected", config.selected);
    cJSON_AddStringToObject(root, "region", config.region.c_str());
    cJSON_AddStringToObject(root, "workspace_id", config.workspace_id.c_str());
    cJSON_AddBoolToObject(root, "api_key_set", !config.api_key.empty());
    cJSON_AddBoolToObject(root, "debug_enabled", config.debug_enabled);
    cJSON_AddStringToObject(root, "debug_address", config.debug_address.c_str());
    cJSON_AddNumberToObject(root, "debug_level", config.debug_level);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
        return Reply(request, "500 Internal Server Error", "{}");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "http://192.168.4.1");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    auto result = httpd_resp_sendstr(request, json);
    cJSON_free(json);
    return result;
}

bool ReadBool(const cJSON* root, const char* name, bool& out) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsBool(item))
        return false;
    out = cJSON_IsTrue(item);
    return true;
}

bool ReadString(const cJSON* root, const char* name, std::string& out) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item))
        return false;
    out = item->valuestring;
    return true;
}

esp_err_t PutConfig(httpd_req_t* request) {
    if (request->content_len <= 0 || request->content_len > 2048) {
        return Reply(request, "413 Content Too Large", "{\"error\":\"invalid_size\"}");
    }
    std::string body(request->content_len, '\0');
    size_t received = 0;
    while (received < body.size()) {
        int count = httpd_req_recv(request, body.data() + received, body.size() - received);
        if (count <= 0)
            return Reply(request, "400 Bad Request", "{\"error\":\"incomplete_body\"}");
        received += count;
    }
    cJSON* root = cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return Reply(request, "400 Bad Request", "{\"error\":\"invalid_json\"}");
    }
    auto config = LoadAliyunRealtimeConfig();
    bool clear_key = false;
    const cJSON* level = cJSON_GetObjectItemCaseSensitive(root, "debug_level");
    const cJSON* key = cJSON_GetObjectItemCaseSensitive(root, "api_key");
    bool valid = ReadBool(root, "selected", config.selected) &&
                 ReadString(root, "region", config.region) &&
                 ReadString(root, "workspace_id", config.workspace_id) &&
                 ReadBool(root, "debug_enabled", config.debug_enabled) &&
                 ReadString(root, "debug_address", config.debug_address) &&
                 ReadBool(root, "clear_api_key", clear_key) && cJSON_IsNumber(level) &&
                 level->valuedouble == level->valueint && cJSON_IsString(key);
    std::string ota;
    const auto* ota_item = cJSON_GetObjectItemCaseSensitive(root, "xiaozhi_ota_url");
    const bool update_ota = ota_item != nullptr;
    if (update_ota)
        valid =
            valid && ReadString(root, "xiaozhi_ota_url", ota) && wifi_config::IsValidOtaUrl(ota);
    if (cJSON_GetObjectItemCaseSensitive(root, "websocket_url"))
        valid = valid && ReadString(root, "websocket_url", config.websocket_url);
    if (cJSON_GetObjectItemCaseSensitive(root, "debug_input_pcm"))
        valid = valid && ReadBool(root, "debug_input_pcm", config.debug_input_pcm);
    if (valid) {
        config.debug_level = level->valueint;
        if (clear_key)
            config.api_key.clear();
        else if (key->valuestring[0] != '\0')
            config.api_key = key->valuestring;
        valid = ValidateAliyunRealtimeConfig(config);
    }
    cJSON_Delete(root);
    if (!valid)
        return Reply(request, "400 Bad Request", "{\"error\":\"invalid_config\"}");
    if (!SaveAliyunRealtimeConfig(config)) {
        return Reply(request, "500 Internal Server Error", "{\"error\":\"save_failed\"}");
    }
    if (update_ota) {
        Settings wifi("wifi", true);
        wifi.SetString("ota_url", ota);
    }
    return Reply(request, "200 OK", "{\"ok\":true,\"restart_required\":true}");
}

esp_err_t Options(httpd_req_t* request) {
    httpd_resp_set_hdr(request, "Access-Control-Allow-Methods", "GET, PUT, POST, OPTIONS");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Headers", "Content-Type");
    return Reply(request, "200 OK", "{}");
}

esp_err_t Provision(httpd_req_t* request) {
    if (!wifi_action)
        return Reply(request, "400 Bad Request", "{}");
    Application::GetInstance().Schedule([]() { wifi_action(); });
    return Reply(request, "202 Accepted", "{\"queued\":true}");
}

esp_err_t Volume(httpd_req_t* request) {
    char query[32] = {}, value[8] = {};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "value", value, sizeof(value)) != ESP_OK)
        return Reply(request, "400 Bad Request", "{}");
    char* end = nullptr;
    const long volume = strtol(value, &end, 10);
    if (!*value || *end || volume < 0 || volume > 100)
        return Reply(request, "400 Bad Request", "{}");
    Application::GetInstance().Schedule([volume]() {
        if (auto codec = Board::GetInstance().GetAudioCodec())
            codec->SetOutputVolume(volume);
    });
    return Reply(request, "202 Accepted", "{\"queued\":true}");
}

esp_err_t Restart(httpd_req_t* request) {
    auto result = Reply(request, "202 Accepted", "{\"queued\":true}");
    xTaskCreate(
        [](void*) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Application::GetInstance().Schedule([]() { Application::GetInstance().Reboot(); });
            vTaskDelete(nullptr);
        },
        "config_restart", 2048, nullptr, 1, nullptr);
    return result;
}

esp_err_t ChatAction(httpd_req_t* request) {
    if (!LoadAliyunRealtimeConfig().debug_enabled)
        return Reply(request, "403 Forbidden", "{}");
    char query[64] = {}, action[16] = {};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "action", action, sizeof(action)) != ESP_OK)
        return Reply(request, "400 Bad Request", "{}");
    auto& app = Application::GetInstance();
    if (strcmp(action, "click") == 0)
        app.ToggleChatState();
    else if (strcmp(action, "cycle") == 0)
        app.CycleListeningMode();
    else if (strcmp(action, "stop") == 0)
        app.EndConversation();
    else
        return Reply(request, "400 Bad Request", "{}");
    return Reply(request, "202 Accepted", "{\"queued\":true}");
}
}  // namespace

void SetWifiConfigAction(std::function<void()> action) { wifi_action = std::move(action); }

void StartAliyunConfigWeb(bool provisioning) {
    if (server && ap_mode == provisioning)
        return;
    StopAliyunConfigWeb();
    ap_mode = provisioning;
    httpd_config_t settings = HTTPD_DEFAULT_CONFIG();
    settings.server_port = provisioning ? 81 : 80;
    settings.ctrl_port = 32769;
    settings.stack_size = 6144;
    settings.max_uri_handlers = 10;
    if (httpd_start(&server, &settings) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to start configuration server");
        server = nullptr;
        return;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = GetPage},
        {.uri = "/api/config", .method = HTTP_GET, .handler = GetConfig},
        {.uri = "/api/config", .method = HTTP_PUT, .handler = PutConfig},
        {.uri = "/api/config", .method = HTTP_OPTIONS, .handler = Options},
        {.uri = "/api/network/provision", .method = HTTP_POST, .handler = Provision},
        {.uri = "/api/volume", .method = HTTP_POST, .handler = Volume},
        {.uri = "/api/restart", .method = HTTP_POST, .handler = Restart},
        {.uri = "/api/test/chat", .method = HTTP_POST, .handler = ChatAction},
    };
    for (const auto& route : routes)
        httpd_register_uri_handler(server, &route);
    ESP_LOGI(kTag, "Configuration page listening on port %d", settings.server_port);
}

void StopAliyunConfigWeb() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
}
