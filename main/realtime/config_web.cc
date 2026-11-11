#include "config_web.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <cJSON.h>

#include <string>
#include "config.h"

namespace {
constexpr char kTag[] = "AliyunConfigWeb";
httpd_handle_t server = nullptr;
extern const char config_html_start[] asm("_binary_config_html_start");
extern const char config_html_end[] asm("_binary_config_html_end");

esp_err_t Reply(httpd_req_t* request, const char* status, const char* body) {
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
    if (request->content_len <= 0 || request->content_len > 1024) {
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
    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
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
    return Reply(request, "200 OK", "{\"ok\":true,\"restart_required\":true}");
}

}  // namespace

void StartAliyunConfigWeb() {
    if (server)
        return;
    httpd_config_t settings = HTTPD_DEFAULT_CONFIG();
    settings.server_port = 80;
    settings.max_uri_handlers = 3;
    if (httpd_start(&server, &settings) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to start configuration server");
        server = nullptr;
        return;
    }
    httpd_uri_t page = {.uri = "/", .method = HTTP_GET, .handler = GetPage};
    httpd_uri_t get = {.uri = "/api/config", .method = HTTP_GET, .handler = GetConfig};
    httpd_uri_t put = {.uri = "/api/config", .method = HTTP_PUT, .handler = PutConfig};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &get);
    httpd_register_uri_handler(server, &put);
    ESP_LOGI(kTag, "Configuration page listening on port 80");
}

void StopAliyunConfigWeb() {
    if (!server)
        return;
    httpd_stop(server);
    server = nullptr;
}
