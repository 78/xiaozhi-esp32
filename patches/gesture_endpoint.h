#ifndef GESTURE_ENDPOINT_H
#define GESTURE_ENDPOINT_H

// Tiny HTTP endpoint so the phone's hand-gesture bridge (phone-mcp/gesture_bridge.py) can drive the
// bot: show an emotion on the OLED eyes, or wake the bot.
//
//   POST http://<bot-ip>:8080/gesture      header  X-Gesture-Key: <key>
//   body  {"emotion":"happy"}   or   {"action":"wake"}   (extra fields such as "gesture" are ignored)
//
// The key is compiled in (GESTURE_KEY, set by the build workflow). Requests without the right key,
// bodies over 256 bytes and unknown emotion names are rejected. An emotion shown this way returns to
// "neutral" after a few seconds so the eyes do not stay stuck.

#include <cstdint>
#include <cstring>
#include <string>

#include <cJSON.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "application.h"
#include "board.h"
#include "display/display.h"

#ifndef GESTURE_KEY
#error "GESTURE_KEY must be defined by the build"
#endif

namespace gesture_endpoint {

static const char* const kTag = "GestureEndpoint";
static httpd_handle_t g_server = nullptr;
static esp_timer_handle_t g_revert_timer = nullptr;

inline bool KeyMatches(const char* given) {
    const char* want = GESTURE_KEY;
    size_t want_len = strlen(want);
    size_t given_len = given ? strlen(given) : 0;
    unsigned char diff = (want_len != given_len);
    for (size_t i = 0; i < want_len; i++) {
        diff |= static_cast<unsigned char>(want[i] ^ (i < given_len ? given[i] : 0));
    }
    return diff == 0;
}

inline bool EmotionAllowed(const std::string& name) {
    static const char* const kAllowed[] = {"neutral", "happy", "sad", "angry", "surprised", "laughing",
                                           "loving", "cool", "relaxed", "confident", "crying", "funny"};
    for (const char* a : kAllowed) {
        if (name == a) return true;
    }
    return false;
}

inline void Revert(void*) {
    Application::GetInstance().Schedule([]() {
        // Only touch the eyes when nothing else is going on
        if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle) {
            auto display = Board::GetInstance().GetDisplay();
            if (display) display->SetEmotion("neutral");
        }
    });
}

inline esp_err_t Reply(httpd_req_t* req, const char* status, const char* body) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

inline esp_err_t Handler(httpd_req_t* req) {
    char key[96] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Gesture-Key", key, sizeof(key)) != ESP_OK || !KeyMatches(key)) {
        return Reply(req, "401 Unauthorized", "{\"ok\":false}");
    }
    if (req->content_len == 0 || req->content_len > 256) {
        return Reply(req, "400 Bad Request", "{\"ok\":false}");
    }
    char body[257];
    size_t got = 0;
    while (got < req->content_len) {
        int n = httpd_req_recv(req, body + got, req->content_len - got);
        if (n <= 0) return ESP_FAIL;
        got += n;
    }
    body[got] = '\0';

    cJSON* json = cJSON_Parse(body);
    if (json == nullptr) {
        return Reply(req, "400 Bad Request", "{\"ok\":false}");
    }
    const cJSON* emotion = cJSON_GetObjectItem(json, "emotion");
    const cJSON* action = cJSON_GetObjectItem(json, "action");
    const char* result = "{\"ok\":false,\"message\":\"nothing to do\"}";
    const char* status = "400 Bad Request";

    if (cJSON_IsString(emotion) && emotion->valuestring && EmotionAllowed(emotion->valuestring)) {
        std::string name = emotion->valuestring;
        Application::GetInstance().Schedule([name]() {
            if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) return;
            auto display = Board::GetInstance().GetDisplay();
            if (display) display->SetEmotion(name.c_str());
            if (g_revert_timer) {
                esp_timer_stop(g_revert_timer);
                if (name != "neutral") esp_timer_start_once(g_revert_timer, 5ULL * 1000 * 1000);
            }
        });
        result = "{\"ok\":true}";
        status = "200 OK";
    } else if (cJSON_IsString(action) && action->valuestring && strcmp(action->valuestring, "wake") == 0) {
        // WakeWordInvoke schedules itself and ignores the call unless the bot can start listening
        Application::GetInstance().WakeWordInvoke("jarvis");
        result = "{\"ok\":true}";
        status = "200 OK";
    }
    cJSON_Delete(json);
    return Reply(req, status, result);
}

// Safe to call more than once (every time the network comes up).
inline void Start() {
    if (g_server != nullptr) return;
    if (g_revert_timer == nullptr) {
        esp_timer_create_args_t args = {};
        args.callback = Revert;
        args.name = "gesture_revert";
        esp_timer_create(&args, &g_revert_timer);
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.max_open_sockets = 3;
    config.stack_size = 6144;
    config.lru_purge_enable = true;
    if (httpd_start(&g_server, &config) != ESP_OK) {
        g_server = nullptr;
        ESP_LOGW(kTag, "could not start the gesture endpoint");
        return;
    }
    httpd_uri_t uri = {};
    uri.uri = "/gesture";
    uri.method = HTTP_POST;
    uri.handler = Handler;
    httpd_register_uri_handler(g_server, &uri);
    ESP_LOGI(kTag, "gesture endpoint on port 8080");
}

}  // namespace gesture_endpoint

#endif  // GESTURE_ENDPOINT_H
