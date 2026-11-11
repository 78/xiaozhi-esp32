#include "local_api_server.h"

#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstring>
#include <memory>

#include "application.h"
#include "board.h"
#include "cjson_utils.h"
#include "device_state_machine.h"
#include "mcp_server.h"

#define TAG "LocalApi"

namespace {

constexpr const char* kStatus200 = "200 OK";
constexpr const char* kStatus202 = "202 Accepted";
constexpr const char* kStatus204 = "204 No Content";
constexpr const char* kStatus400 = "400 Bad Request";
constexpr const char* kStatus401 = "401 Unauthorized";
constexpr const char* kStatus404 = "404 Not Found";
constexpr const char* kStatus408 = "408 Request Timeout";
constexpr const char* kStatus413 = "413 Payload Too Large";
constexpr const char* kStatus500 = "500 Internal Server Error";
constexpr const char* kStatus503 = "503 Service Unavailable";

constexpr const char* kToolsPrefix = "/api/tools/";
// Default text sent as the "wake word" by /api/chat/start when the body has none.
constexpr const char* kDefaultWakeText = "你好小智";

std::atomic<int> g_next_request_id{100000};

std::string PrintJson(const cJSON* json) {
    CJsonStringUniquePtr text(cJSON_PrintUnformatted(json));
    return text ? std::string(text.get()) : std::string("{}");
}

// Comparison whose duration does not depend on where the strings differ.
bool SecureEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::optional<std::string> GetHeader(httpd_req_t* req, const char* name) {
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0) {
        return std::nullopt;
    }
    std::string value(len + 1, '\0');
    if (httpd_req_get_hdr_value_str(req, name, value.data(), value.size()) != ESP_OK) {
        return std::nullopt;
    }
    value.resize(len);
    return value;
}

std::string GetIpAddress() {
    for (const char* key : {"WIFI_STA_DEF", "ETH_DEF"}) {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey(key);
        esp_netif_ip_info_t ip_info;
        if (netif != nullptr && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK &&
            ip_info.ip.addr != 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
            return buf;
        }
    }
    return "";
}

const char* StatusForMcpError(int code) {
    switch (code) {
        case -32600:  // Invalid request
        case -32602:  // Invalid params / unknown tool / bad argument
            return kStatus400;
        case -32601:  // Method not found
            return kStatus404;
        default:
            return kStatus500;
    }
}

}  // namespace

bool LocalApiServer::Start() {
    if (server_ != nullptr) {
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_LOCAL_API_PORT;
    config.ctrl_port = CONFIG_LOCAL_API_CTRL_PORT;
    config.max_open_sockets = 3;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 6144;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server on port %d: %s", CONFIG_LOCAL_API_PORT,
                 esp_err_to_name(err));
        server_ = nullptr;
        return false;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/api/info", .method = HTTP_GET, .handler = HandleInfo, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = HandleStatus, .user_ctx = nullptr},
        {.uri = "/api/tools", .method = HTTP_GET, .handler = HandleToolsList, .user_ctx = nullptr},
        {.uri = "/api/tools/*", .method = HTTP_POST, .handler = HandleToolCall, .user_ctx = nullptr},
        {.uri = "/api/mcp", .method = HTTP_POST, .handler = HandleMcp, .user_ctx = nullptr},
        {.uri = "/api/chat/start", .method = HTTP_POST, .handler = HandleChatStart, .user_ctx = nullptr},
        {.uri = "/api/chat/toggle", .method = HTTP_POST, .handler = HandleChatToggle, .user_ctx = nullptr},
        {.uri = "/api/chat/abort", .method = HTTP_POST, .handler = HandleChatAbort, .user_ctx = nullptr},
        {.uri = "/api/*", .method = HTTP_OPTIONS, .handler = HandleOptions, .user_ctx = nullptr},
    };
    for (const auto& route : routes) {
        err = httpd_register_uri_handler(server_, &route);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s: %s", route.uri, esp_err_to_name(err));
        }
    }

    httpd_register_err_handler(server_, HTTPD_404_NOT_FOUND,
                               [](httpd_req_t* req, httpd_err_code_t) -> esp_err_t {
                                   return SendError(req, kStatus404, "Route not found");
                               });

    if (strlen(CONFIG_LOCAL_API_TOKEN) == 0) {
        ESP_LOGW(TAG, "Local API has no token: anyone on the network can control the device");
    }
    ESP_LOGI(TAG, "Local API listening on http://%s:%d/api", GetIpAddress().c_str(),
             CONFIG_LOCAL_API_PORT);
    return true;
}

void LocalApiServer::Stop() {
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
        ESP_LOGI(TAG, "Local API stopped");
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::optional<std::string> LocalApiServer::RunOnMainTask(
    std::function<void(std::function<void(const std::string&)>)> fn, int timeout_ms) {
    // Shared with the main task so a late reply after a timeout stays safe.
    struct Pending {
        SemaphoreHandle_t semaphore = xSemaphoreCreateBinary();
        std::atomic<bool> done{false};
        std::string result;
        ~Pending() {
            if (semaphore != nullptr) {
                vSemaphoreDelete(semaphore);
            }
        }
    };
    auto pending = std::make_shared<Pending>();
    if (pending->semaphore == nullptr) {
        return std::nullopt;
    }

    Application::GetInstance().Schedule([pending, fn = std::move(fn)]() {
        fn([pending](const std::string& result) {
            if (pending->done.exchange(true)) {
                return;  // Only the first reply counts
            }
            pending->result = result;
            xSemaphoreGive(pending->semaphore);
        });
    });

    if (xSemaphoreTake(pending->semaphore, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return std::nullopt;
    }
    return std::move(pending->result);
}

std::optional<std::string> LocalApiServer::CallMcp(const std::string& request) {
    // The request must already be valid JSON-RPC with a numeric id: McpServer drops
    // invalid messages silently, which would otherwise show up as a timeout here.
    return RunOnMainTask(
        [request](std::function<void(const std::string&)> reply) {
            McpServer::GetInstance().ParseMessage(request, std::move(reply));
        },
        CONFIG_LOCAL_API_TIMEOUT_MS);
}

bool LocalApiServer::CheckAuth(httpd_req_t* req) {
    const std::string token = CONFIG_LOCAL_API_TOKEN;
    if (token.empty()) {
        return true;
    }
    if (auto auth = GetHeader(req, "Authorization")) {
        if (SecureEquals(*auth, "Bearer " + token)) {
            return true;
        }
    }
    if (auto header = GetHeader(req, "X-Api-Token")) {
        if (SecureEquals(*header, token)) {
            return true;
        }
    }
    return false;
}

void LocalApiServer::SetCommonHeaders(httpd_req_t* req) {
#if CONFIG_LOCAL_API_ENABLE_CORS
    // httpd keeps the pointers, so only string literals are used here.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                       "Authorization, Content-Type, X-Api-Token");
#endif
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

esp_err_t LocalApiServer::SendJson(httpd_req_t* req, const char* status, const std::string& body) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    SetCommonHeaders(req);
    return httpd_resp_send(req, body.data(), body.size());
}

esp_err_t LocalApiServer::SendError(httpd_req_t* req, const char* status,
                                    const std::string& message) {
    CJsonUniquePtr json(cJSON_CreateObject());
    cJSON_AddStringToObject(json.get(), "error", message.c_str());
    return SendJson(req, status, PrintJson(json.get()));
}

std::optional<std::string> LocalApiServer::ReadBody(httpd_req_t* req, std::string& error_status) {
    if (req->content_len > CONFIG_LOCAL_API_MAX_BODY) {
        error_status = kStatus413;
        return std::nullopt;
    }
    std::string body(req->content_len, '\0');
    size_t received = 0;
    int timeouts = 0;
    while (received < body.size()) {
        int ret = httpd_req_recv(req, body.data() + received, body.size() - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) {
            continue;
        }
        if (ret <= 0) {
            error_status = ret == HTTPD_SOCK_ERR_TIMEOUT ? kStatus408 : kStatus400;
            return std::nullopt;
        }
        received += ret;
    }
    return body;
}

// Parses the request body into a JSON object. An empty body yields an empty object.
// On failure an error response has already been sent and nullptr is returned.
static CJsonUniquePtr ParseObjectBody(httpd_req_t* req, const std::optional<std::string>& body,
                                      const std::string& read_error,
                                      esp_err_t (*send_error)(httpd_req_t*, const char*,
                                                              const std::string&)) {
    if (!body) {
        send_error(req, read_error.c_str(), "Could not read request body");
        return nullptr;
    }
    if (body->find_first_not_of(" \t\r\n") == std::string::npos) {
        return CJsonUniquePtr(cJSON_CreateObject());
    }
    CJsonUniquePtr json(cJSON_ParseWithLength(body->data(), body->size()));
    if (!json || !cJSON_IsObject(json.get())) {
        send_error(req, "400 Bad Request", "Body must be a JSON object");
        return nullptr;
    }
    return json;
}

#define REQUIRE_AUTH(req)                                              \
    do {                                                               \
        if (!CheckAuth(req)) {                                         \
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");     \
            return SendError(req, kStatus401, "Invalid or missing token"); \
        }                                                              \
    } while (0)

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

esp_err_t LocalApiServer::HandleOptions(httpd_req_t* req) {
    httpd_resp_set_status(req, kStatus204);
    SetCommonHeaders(req);
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t LocalApiServer::HandleInfo(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    CJsonUniquePtr json(cJSON_CreateObject());
    cJSON_AddStringToObject(json.get(), "name", BOARD_NAME);
    cJSON_AddStringToObject(json.get(), "board_type", board.GetBoardType().c_str());
    cJSON_AddStringToObject(json.get(), "uuid", board.GetUuid().c_str());
    cJSON_AddStringToObject(json.get(), "version", app_desc->version);
    cJSON_AddStringToObject(json.get(), "idf_version", app_desc->idf_ver);
    cJSON_AddStringToObject(json.get(), "state",
                            DeviceStateMachine::GetStateName(app.GetDeviceState()));
    cJSON_AddStringToObject(json.get(), "ip", GetIpAddress().c_str());
    cJSON_AddNumberToObject(json.get(), "uptime_s", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(json.get(), "free_internal",
                            heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(json.get(), "min_free_internal",
                            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(json.get(), "free_psram", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return SendJson(req, kStatus200, PrintJson(json.get()));
}

esp_err_t LocalApiServer::HandleStatus(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    auto status = RunOnMainTask(
        [](std::function<void(const std::string&)> reply) {
            reply(Board::GetInstance().GetDeviceStatusJson());
        },
        CONFIG_LOCAL_API_TIMEOUT_MS);
    if (!status) {
        return SendError(req, kStatus503, "Device busy (timeout)");
    }
    return SendJson(req, kStatus200, *status);
}

esp_err_t LocalApiServer::HandleToolsList(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    // McpServer paginates tools/list; follow nextCursor and merge everything.
    CJsonUniquePtr tools(cJSON_CreateArray());
    std::string cursor;
    for (int page = 0; page < 16; ++page) {
        CJsonUniquePtr request(cJSON_CreateObject());
        cJSON_AddStringToObject(request.get(), "jsonrpc", "2.0");
        cJSON_AddNumberToObject(request.get(), "id", g_next_request_id++);
        cJSON_AddStringToObject(request.get(), "method", "tools/list");
        cJSON* params = cJSON_AddObjectToObject(request.get(), "params");
        cJSON_AddStringToObject(params, "cursor", cursor.c_str());
        cJSON_AddBoolToObject(params, "withUserTools", true);

        auto response = CallMcp(PrintJson(request.get()));
        if (!response) {
            return SendError(req, kStatus503, "Device busy (timeout)");
        }
        CJsonUniquePtr json(cJSON_Parse(response->c_str()));
        cJSON* result = json ? cJSON_GetObjectItem(json.get(), "result") : nullptr;
        cJSON* page_tools = result ? cJSON_GetObjectItem(result, "tools") : nullptr;
        if (!cJSON_IsArray(page_tools)) {
            return SendError(req, kStatus500, "Invalid tools/list response");
        }
        while (cJSON_GetArraySize(page_tools) > 0) {
            cJSON_AddItemToArray(tools.get(), cJSON_DetachItemFromArray(page_tools, 0));
        }
        cJSON* next = cJSON_GetObjectItem(result, "nextCursor");
        if (!cJSON_IsString(next) || next->valuestring[0] == '\0') {
            break;
        }
        cursor = next->valuestring;
    }

    CJsonUniquePtr out(cJSON_CreateObject());
    cJSON_AddItemToObject(out.get(), "tools", tools.release());
    return SendJson(req, kStatus200, PrintJson(out.get()));
}

esp_err_t LocalApiServer::HandleToolCall(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    std::string name(req->uri);
    if (name.rfind(kToolsPrefix, 0) != 0) {
        return SendError(req, kStatus400, "Missing tool name");
    }
    name = name.substr(strlen(kToolsPrefix));
    auto query = name.find('?');
    if (query != std::string::npos) {
        name.resize(query);
    }
    if (name.empty()) {
        return SendError(req, kStatus400, "Missing tool name: POST /api/tools/<name>");
    }

    std::string read_error;
    auto body = ReadBody(req, read_error);
    auto arguments = ParseObjectBody(req, body, read_error, SendError);
    if (!arguments) {
        return ESP_OK;
    }

    CJsonUniquePtr request(cJSON_CreateObject());
    cJSON_AddStringToObject(request.get(), "jsonrpc", "2.0");
    cJSON_AddNumberToObject(request.get(), "id", g_next_request_id++);
    cJSON_AddStringToObject(request.get(), "method", "tools/call");
    cJSON* params = cJSON_AddObjectToObject(request.get(), "params");
    cJSON_AddStringToObject(params, "name", name.c_str());
    cJSON_AddItemToObject(params, "arguments", arguments.release());

    auto response = CallMcp(PrintJson(request.get()));
    if (!response) {
        return SendError(req, kStatus503, "Tool call timed out");
    }

    CJsonUniquePtr json(cJSON_Parse(response->c_str()));
    if (!json) {
        return SendError(req, kStatus500, "Invalid MCP response");
    }
    cJSON* error = cJSON_GetObjectItem(json.get(), "error");
    if (cJSON_IsObject(error)) {
        cJSON* code = cJSON_GetObjectItem(error, "code");
        cJSON* message = cJSON_GetObjectItem(error, "message");
        return SendError(req, StatusForMcpError(cJSON_IsNumber(code) ? code->valueint : 0),
                         cJSON_IsString(message) ? message->valuestring : "Tool call failed");
    }
    cJSON* result = cJSON_GetObjectItem(json.get(), "result");
    return SendJson(req, kStatus200, result ? PrintJson(result) : std::string("{}"));
}

esp_err_t LocalApiServer::HandleMcp(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    std::string read_error;
    auto body = ReadBody(req, read_error);
    if (!body) {
        return SendError(req, read_error.c_str(), "Could not read request body");
    }

    // Validate up front: McpServer silently ignores malformed messages.
    CJsonUniquePtr json(cJSON_ParseWithLength(body->data(), body->size()));
    if (!json || !cJSON_IsObject(json.get())) {
        return SendError(req, kStatus400, "Body must be a JSON-RPC 2.0 object (batches unsupported)");
    }
    cJSON* version = cJSON_GetObjectItem(json.get(), "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(json.get(), "method");
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method)) {
        return SendError(req, kStatus400, "Expected \"jsonrpc\":\"2.0\" and a \"method\"");
    }
    if (strncmp(method->valuestring, "notifications", strlen("notifications")) == 0) {
        httpd_resp_set_status(req, kStatus202);
        SetCommonHeaders(req);
        return httpd_resp_send(req, nullptr, 0);
    }
    if (!cJSON_IsNumber(cJSON_GetObjectItem(json.get(), "id"))) {
        return SendError(req, kStatus400, "\"id\" must be a number");
    }
    cJSON* params = cJSON_GetObjectItem(json.get(), "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        return SendError(req, kStatus400, "\"params\" must be an object");
    }

    auto response = CallMcp(*body);
    if (!response) {
        return SendError(req, kStatus503, "MCP request timed out");
    }
    return SendJson(req, kStatus200, *response);
}

esp_err_t LocalApiServer::HandleChatStart(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    std::string read_error;
    auto body = ReadBody(req, read_error);
    auto json = ParseObjectBody(req, body, read_error, SendError);
    if (!json) {
        return ESP_OK;
    }
    cJSON* text = cJSON_GetObjectItem(json.get(), "text");
    std::string wake_text =
        cJSON_IsString(text) && text->valuestring[0] != '\0' ? text->valuestring : kDefaultWakeText;

    auto& app = Application::GetInstance();
    app.Schedule([wake_text]() { Application::GetInstance().WakeWordInvoke(wake_text); });
    return SendJson(req, kStatus202, "{\"ok\":true}");
}

esp_err_t LocalApiServer::HandleChatToggle(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    Application::GetInstance().ToggleChatState();  // Only sets an event bit: thread-safe
    return SendJson(req, kStatus202, "{\"ok\":true}");
}

esp_err_t LocalApiServer::HandleChatAbort(httpd_req_t* req) {
    REQUIRE_AUTH(req);
    Application::GetInstance().Schedule(
        []() { Application::GetInstance().AbortSpeaking(kAbortReasonNone); });
    return SendJson(req, kStatus202, "{\"ok\":true}");
}
