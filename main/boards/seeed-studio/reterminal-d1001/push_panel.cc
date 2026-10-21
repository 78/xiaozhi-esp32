#include "push_panel.h"
#include "application.h"
#include "settings.h"

#include "application.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstring>

static const char* TAG = "PushPanel";

// 30px CJK font (common-glyph subset) for headings and ?size=large bodies;
// linked from the xiaozhi-fonts component.
LV_FONT_DECLARE(font_noto_sans_basic_30_4);

namespace {

constexpr size_t kMaxBodyBytes = 32 * 1024;
constexpr int kDefaultChoiceTimeoutS = 30;
constexpr int kMaxChoiceTimeoutS = 60;
constexpr int kHeaderHeight = 48;
constexpr size_t kRemoteWakeMaxBody = 2048;
constexpr size_t kRemoteWakeContextMax = 25;
constexpr size_t kRemoteWakeSpeakMax = 512;
constexpr int64_t kRemoteWakeDedupeUs = 15000000;
std::string RemoteWakeToken() {
    Settings s("remote_wake", false);
    return s.GetString("token", "");
}
bool RemoteWakeEnabled() {
    Settings s("remote_wake", false);
    return s.GetBool("enabled", false) && !s.GetString("token", "").empty();
}
bool HasRemoteWakeAuth(httpd_req_t* req) {
    std::string expected = RemoteWakeToken();
    size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (expected.empty() || len < 7 || len > 255)
        return false;
    std::string got(len + 1, '\0');
    if (httpd_req_get_hdr_value_str(req, "Authorization", got.data(), got.size()) != ESP_OK)
        return false;
    got.resize(len);
    return got.compare(0, 7, "Bearer ") == 0 && got.substr(7) == expected;
}
bool ValidUtf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const uint8_t c = static_cast<uint8_t>(s[i++]);
        if (c == 0)
            return false;
        if (c <= 0x7f)
            continue;
        size_t n = c >= 0xc2 && c <= 0xdf   ? 1
                   : c >= 0xe0 && c <= 0xef ? 2
                   : c >= 0xf0 && c <= 0xf4 ? 3
                                            : 0;
        if (n == 0 || i + n > s.size())
            return false;
        uint32_t cp = c & ((1u << (6 - n)) - 1u);
        for (size_t j = 0; j < n; ++j) {
            const uint8_t d = static_cast<uint8_t>(s[i++]);
            if ((d & 0xc0) != 0x80)
                return false;
            cp = (cp << 6) | (d & 0x3f);
        }
        if ((n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) ||
            cp > 0x10ffff)
            return false;
    }
    return true;
}
bool ContainsJsonNulEscape(const std::string& body) {
    for (size_t pos = 0; (pos = body.find("u0000", pos)) != std::string::npos; ++pos) {
        size_t slashes = 0;
        for (size_t p = pos; p > 0 && body[p - 1] == '\\'; --p)
            ++slashes;
        if ((slashes & 1u) != 0)
            return true;
    }
    return false;
}
const char* RemoteWakeState(DeviceState state) {
    switch (state) {
        case kDeviceStateIdle:
            return "idle";
        case kDeviceStateSpeaking:
            return "speaking";
        case kDeviceStateListening:
            return "listening";
        case kDeviceStateConnecting:
            return "connecting";
        default:
            return "busy";
    }
}

// Same accent as the settings overlay's primary actions.
lv_color_t AccentColor() {
    return lv_color_hex(0x8CC020);
}

// Accent used for text on the theme background: green stays legible on the
// dark theme, while the light theme needs the deeper green.
lv_color_t AccentTextColor() {
    lv_color_t bg = lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN);
    return lv_color_brightness(bg) > 128 ? lv_color_hex(0x3B5A11) : AccentColor();
}

// Strips markdown emphasis markers in place ("**bold**", "`code`").
std::string StripInline(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '*' || c == '`') {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

// Splits one markdown table row into trimmed cells.
std::vector<std::string> SplitTableRow(const std::string& line) {
    std::vector<std::string> cells;
    size_t start = line.find('|');
    if (start == std::string::npos) {
        return cells;
    }
    ++start;
    while (start <= line.size()) {
        size_t end = line.find('|', start);
        if (end == std::string::npos) {
            end = line.size();
        }
        std::string cell = line.substr(start, end - start);
        // Trim.
        size_t b = cell.find_first_not_of(" \t");
        size_t e = cell.find_last_not_of(" \t");
        cells.push_back(b == std::string::npos ? "" : cell.substr(b, e - b + 1));
        start = end + 1;
    }
    // A trailing '|' leaves one empty phantom cell.
    if (!cells.empty() && cells.back().empty()) {
        cells.pop_back();
    }
    return cells;
}

bool IsSeparatorRow(const std::vector<std::string>& cells) {
    if (cells.empty()) {
        return false;
    }
    for (const auto& cell : cells) {
        if (cell.find_first_not_of("-: \t") != std::string::npos) {
            return false;
        }
    }
    return true;
}

// Cap for the floating card: 55% of the current (rotation-aware) height.
int32_t MaxCardHeight() {
    return lv_display_get_vertical_resolution(lv_display_get_default()) * 55 / 100;
}

// Styles the table's first row as a header: accent fill, white text.
void TableDrawTask(lv_event_t* event) {
    lv_draw_task_t* task = lv_event_get_draw_task(event);
    auto* base = static_cast<lv_draw_dsc_base_t*>(lv_draw_task_get_draw_dsc(task));
    if (base == nullptr || base->part != LV_PART_ITEMS || base->id1 != 0) {
        return;
    }
    lv_draw_task_type_t type = lv_draw_task_get_type(task);
    if (type == LV_DRAW_TASK_TYPE_FILL) {
        lv_draw_fill_dsc_t* fill = lv_draw_task_get_fill_dsc(task);
        if (fill != nullptr) {
            fill->color = AccentColor();
            fill->opa = LV_OPA_COVER;
        }
    } else if (type == LV_DRAW_TASK_TYPE_LABEL) {
        lv_draw_label_dsc_t* label = lv_draw_task_get_label_dsc(task);
        if (label != nullptr) {
            label->color = lv_color_white();
        }
    }
}

}  // namespace

PushPanel::PushPanel(LcdDisplay* display) : display_(display) {
    choice_sem_ = xSemaphoreCreateBinary();
}

PushPanel::~PushPanel() {
    if (server_ != nullptr) {
        httpd_stop(server_);
    }
    if (choice_sem_ != nullptr) {
        vSemaphoreDelete(choice_sem_);
    }
}

void PushPanel::Start() {
    if (server_ != nullptr) {
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // 8080: the Wi-Fi config-AP mode starts its own HTTP server on port 80,
    // and binding it too made that path abort (listen errno 112) in a reboot
    // loop whenever the station could not connect.
    config.server_port = 8080;
    config.ctrl_port = 32770;  // distinct from default 32768 used by softap/provisioning
    config.stack_size = 10240;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;
    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        server_ = nullptr;
        return;
    }
    const httpd_uri_t handlers[] = {
        {.uri = "/panel/markdown", .method = HTTP_POST, .handler = MarkdownThunk, .user_ctx = this},
        {.uri = "/panel/choice", .method = HTTP_POST, .handler = ChoiceThunk, .user_ctx = this},
        {.uri = "/panel/close", .method = HTTP_POST, .handler = CloseThunk, .user_ctx = this},
        {.uri = "/", .method = HTTP_GET, .handler = UsageThunk, .user_ctx = this},
        {.uri = "/camera/snap", .method = HTTP_GET, .handler = SnapThunk, .user_ctx = this},
        {.uri = "/camera/tune", .method = HTTP_POST, .handler = TuneThunk, .user_ctx = this},
        {.uri = "/camera/raw", .method = HTTP_GET, .handler = RawThunk, .user_ctx = this},
        {.uri = "/face/status", .method = HTTP_GET, .handler = FaceStatusThunk, .user_ctx = this},
        {.uri = "/face/config", .method = HTTP_POST, .handler = FaceConfigThunk, .user_ctx = this},
        {.uri = "/api/wake/status",
         .method = HTTP_GET,
         .handler = WakeStatusThunk,
         .user_ctx = this},
        {.uri = "/api/wake", .method = HTTP_POST, .handler = WakeThunk, .user_ctx = this},
    };
    for (const auto& h : handlers) {
        esp_err_t route_err = httpd_register_uri_handler(server_, &h);
        if (route_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register route %s: %s", h.uri, esp_err_to_name(route_err));
            httpd_stop(server_);
            server_ = nullptr;
            return;
        }
    }
    ESP_LOGI(TAG, "push panel listening on port %d", config.server_port);
}

esp_err_t PushPanel::MarkdownThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleMarkdown(req);
}

esp_err_t PushPanel::ChoiceThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleChoice(req);
}

esp_err_t PushPanel::CloseThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleClose(req);
}

esp_err_t PushPanel::SnapThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleSnap(req);
}

esp_err_t PushPanel::TuneThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleTune(req);
}

esp_err_t PushPanel::HandleSnap(httpd_req_t* req) {
    if (!camera_snap_) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no camera");
        return ESP_FAIL;
    }
    std::vector<uint8_t> jpeg;
    if (!camera_snap_(jpeg) || jpeg.empty()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    return httpd_resp_send(req, reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
}

esp_err_t PushPanel::HandleTune(httpd_req_t* req) {
    if (!camera_tune_) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no camera");
        return ESP_FAIL;
    }
    int exp_pct = -1, gain_idx = -1, red = -1, blue = -1;
    char query[96];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char value[12];
        if (httpd_query_key_value(query, "exp_pct", value, sizeof(value)) == ESP_OK) {
            exp_pct = atoi(value);
        }
        if (httpd_query_key_value(query, "gain_idx", value, sizeof(value)) == ESP_OK) {
            gain_idx = atoi(value);
        }
        if (httpd_query_key_value(query, "r", value, sizeof(value)) == ESP_OK) {
            red = atoi(value);
        }
        if (httpd_query_key_value(query, "b", value, sizeof(value)) == ESP_OK) {
            blue = atoi(value);
        }
    }
    bool ok = camera_tune_(exp_pct, gain_idx, red, blue);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, ok ? "OK\n" : "FAILED\n", HTTPD_RESP_USE_STRLEN);
}

esp_err_t PushPanel::RawThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleRaw(req);
}

esp_err_t PushPanel::HandleRaw(httpd_req_t* req) {
    if (!camera_raw_) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no camera");
        return ESP_FAIL;
    }
    std::vector<uint8_t> bytes;
    std::string meta;
    if (!camera_raw_(bytes, meta) || bytes.empty()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "X-Frame-Meta", meta.c_str());
    return httpd_resp_send(req, reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

esp_err_t PushPanel::FaceStatusThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleFaceStatus(req);
}

esp_err_t PushPanel::FaceConfigThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleFaceConfig(req);
}

esp_err_t PushPanel::HandleFaceStatus(httpd_req_t* req) {
    if (!face_status_) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no face service");
        return ESP_FAIL;
    }
    std::string json = face_status_();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t PushPanel::HandleFaceConfig(httpd_req_t* req) {
    if (!face_config_) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no face service");
        return ESP_FAIL;
    }
    std::string body;
    if (!ReadBody(req, &body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty or oversized body");
        return ESP_FAIL;
    }
    std::string error;
    if (!face_config_(body, &error)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error.c_str());
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK\n", HTTPD_RESP_USE_STRLEN);
}

esp_err_t PushPanel::WakeStatusThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleWakeStatus(req);
}
esp_err_t PushPanel::WakeThunk(httpd_req_t* req) {
    return static_cast<PushPanel*>(req->user_ctx)->HandleWake(req);
}

esp_err_t PushPanel::HandleWakeStatus(httpd_req_t* req) {
    const bool enabled = RemoteWakeEnabled();
    const bool authenticated = !enabled || HasRemoteWakeAuth(req);
    if (!authenticated) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"unauthorized\",\"authenticated\":false}");
    }
    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "state",
                            RemoteWakeState(Application::GetInstance().GetDeviceState()));
    cJSON_AddBoolToObject(r, "enabled", enabled);
    cJSON_AddBoolToObject(r, "authenticated", authenticated);
    if (!enabled)
        cJSON_AddStringToObject(r, "reason", "not_configured");
    cJSON* c = cJSON_AddObjectToObject(r, "capabilities");
    cJSON_AddNumberToObject(c, "max_context_bytes", kRemoteWakeContextMax);
    cJSON_AddNumberToObject(c, "max_speak_bytes", kRemoteWakeSpeakMax);
#if CONFIG_SEND_WAKE_WORD_DATA
    cJSON_AddBoolToObject(c, "context", enabled);
    cJSON_AddBoolToObject(c, "speak", enabled);
    cJSON_AddStringToObject(c, "speak_backend", "local_server_device_call_tts");
#else
    cJSON_AddBoolToObject(c, "context", false);
    cJSON_AddBoolToObject(c, "speak", false);
    cJSON_AddStringToObject(c, "reason", "send_wake_word_data_disabled");
#endif
    char* out = cJSON_PrintUnformatted(r);
    httpd_resp_set_type(req, "application/json");
    esp_err_t e = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    cJSON_Delete(r);
    return e;
}

esp_err_t PushPanel::HandleWake(httpd_req_t* req) {
    if (!RemoteWakeEnabled()) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "remote wake disabled");
        return ESP_FAIL;
    }
    if (!HasRemoteWakeAuth(req)) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "unauthorized");
        return ESP_FAIL;
    }
#if !CONFIG_SEND_WAKE_WORD_DATA
    httpd_resp_set_status(req, "501 Not Implemented");
    httpd_resp_sendstr(req, "wake word data is disabled");
    return ESP_FAIL;
#else
    if (req->content_len == 0 || req->content_len > kRemoteWakeMaxBody) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }
    std::string body(req->content_len, '\0');
    size_t off = 0;
    while (off < body.size()) {
        int n = httpd_req_recv(req, body.data() + off, body.size() - off);
        if (n <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
            return ESP_FAIL;
        }
        off += n;
    }
    const char* end = nullptr;
    cJSON* root = cJSON_ParseWithLengthOpts(body.data(), body.size(), &end, 0);
    cJSON* id = root ? cJSON_GetObjectItem(root, "request_id") : nullptr;
    cJSON* mode = root ? cJSON_GetObjectItem(root, "mode") : nullptr;
    cJSON* text = root ? cJSON_GetObjectItem(root, "text") : nullptr;
    cJSON* ttl = root ? cJSON_GetObjectItem(root, "ttl_ms") : nullptr;
    if (ContainsJsonNulEscape(body) || !root || !cJSON_IsObject(root) ||
        end != body.data() + body.size() || !cJSON_IsString(id) || !cJSON_IsString(mode) ||
        !cJSON_IsString(text) || !cJSON_IsNumber(ttl) || ttl->valuedouble != ttl->valueint ||
        ttl->valueint < 1 || ttl->valueint > 15000 || strlen(id->valuestring) == 0 ||
        strlen(id->valuestring) > 96 ||
        (strcmp(mode->valuestring, "context") && strcmp(mode->valuestring, "speak")) ||
        strlen(text->valuestring) == 0) {
        if (root)
            cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid wake request");
        return ESP_FAIL;
    }
    int ttl_ms = ttl->valueint;
    std::string rid = id->valuestring, m = mode->valuestring, payload = text->valuestring;
    cJSON_Delete(root);
    if (!ValidUtf8(payload) ||
        payload.size() > (m == "context" ? kRemoteWakeContextMax : kRemoteWakeSpeakMax)) {
        httpd_resp_set_status(req, "413 Content Too Large");
        httpd_resp_sendstr(req, "text too long or invalid UTF-8");
        return ESP_FAIL;
    }
    int64_t now = esp_timer_get_time();
    const std::string fingerprint = m + "\n" + payload;
    {
        std::lock_guard<std::mutex> lock(wake_request_mutex_);
        while (!wake_request_history_.empty() &&
               now - wake_request_history_.front().accepted_us >= kRemoteWakeDedupeUs)
            wake_request_history_.pop_front();
        for (const auto& record : wake_request_history_)
            if (record.id == rid) {
                httpd_resp_set_type(req, "application/json");
                if (record.fingerprint == fingerprint) {
                    httpd_resp_set_status(req, "202 Accepted");
                    return httpd_resp_sendstr(req, "{\"status\":\"duplicate\"}");
                }
                httpd_resp_set_status(req, "409 Conflict");
                return httpd_resp_sendstr(req, "{\"error\":\"request_id_conflict\"}");
            }
    }
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateIdle) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"device_busy\"}");
    }
    std::string wake_text = (m == "speak") ? "[device_call] " + payload : payload;
    app.Schedule([&app, wake_text, now, ttl_ms]() {
        if (app.GetDeviceState() == kDeviceStateIdle && (esp_timer_get_time() <= now + ttl_ms * 1000LL)) {
            app.WakeWordInvoke(wake_text);
        }
    });
    {
        std::lock_guard<std::mutex> lock(wake_request_mutex_);
        wake_request_history_.push_back({rid, fingerprint, now});
        while (wake_request_history_.size() > 32)
            wake_request_history_.pop_front();
    }
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"accepted\"}");
#endif
}

esp_err_t PushPanel::UsageThunk(httpd_req_t* req) {
    static const char kUsage[] =
        "reTerminal D1001 push panel\n"
        "  POST /panel/markdown   render markdown; ?ttl_s=30 auto-close,\n"
        "                         ?size=large for 30px body text\n"
        "  POST /panel/choice     {\"title\":\"...\",\"options\":[...],\"timeout_s\":60}\n"
        "                         blocks until the user picks an option\n"
        "  POST /panel/close      close the panel\n";
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, kUsage, HTTPD_RESP_USE_STRLEN);
}

bool PushPanel::ReadBody(httpd_req_t* req, std::string* body) {
    if (req->content_len == 0 || req->content_len > kMaxBodyBytes) {
        return false;
    }
    body->resize(req->content_len);
    size_t received = 0;
    while (received < body->size()) {
        int ret = httpd_req_recv(req, body->data() + received, body->size() - received);
        if (ret <= 0) {
            return false;
        }
        received += ret;
    }
    return true;
}

esp_err_t PushPanel::HandleMarkdown(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, &body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty or oversized body");
        return ESP_FAIL;
    }
    // Optional query: ?ttl_s=30 auto-dismisses, ?size=large uses the 30px font
    // for body text (headings always use it).
    int ttl_s = 0;
    bool large_text = false;
    char query[96];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char value[16];
        if (httpd_query_key_value(query, "ttl_s", value, sizeof(value)) == ESP_OK) {
            ttl_s = std::clamp(atoi(value), 0, 3600);
        }
        if (httpd_query_key_value(query, "size", value, sizeof(value)) == ESP_OK) {
            large_text = strcmp(value, "large") == 0;
        }
    }
    {
        DisplayLockGuard lock(display_);
        // A pending choice keeps the screen; markdown replaces everything else.
        if (choice_pending_) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "a choice is pending");
            return ESP_FAIL;
        }
        large_text_ = large_text;
        OpenRoot("推送内容");
        RenderMarkdown(body);
        ArmTtl(ttl_s);
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK\n", HTTPD_RESP_USE_STRLEN);
}

esp_err_t PushPanel::HandleChoice(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, &body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty or oversized body");
        return ESP_FAIL;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }
    std::string title = "请选择";
    cJSON* title_item = cJSON_GetObjectItem(root, "title");
    if (cJSON_IsString(title_item)) {
        title = title_item->valuestring;
    }
    int timeout_s = kDefaultChoiceTimeoutS;
    cJSON* timeout_item = cJSON_GetObjectItem(root, "timeout_s");
    if (cJSON_IsNumber(timeout_item) && timeout_item->valueint > 0) {
        timeout_s = std::min(timeout_item->valueint, kMaxChoiceTimeoutS);
    }
    std::vector<std::string> options;
    cJSON* options_item = cJSON_GetObjectItem(root, "options");
    if (cJSON_IsArray(options_item)) {
        cJSON* option = nullptr;
        cJSON_ArrayForEach(option, options_item) {
            if (cJSON_IsString(option)) {
                options.push_back(option->valuestring);
            }
        }
    }
    cJSON_Delete(root);
    if (options.empty() || options.size() > 16) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "need 1..16 string options");
        return ESP_FAIL;
    }

    {
        DisplayLockGuard lock(display_);
        if (choice_pending_) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "a choice is already pending");
            return ESP_FAIL;
        }
        ArmTtl(0);
        large_text_ = false;
        choice_options_ = options;
        choice_selected_ = -2;  // -2 = still waiting, -1 = dismissed
        choice_pending_ = true;
        xSemaphoreTake(choice_sem_, 0);  // drain a stale give, if any

        OpenRoot(title.c_str());
        for (const auto& option : choice_options_) {
            lv_obj_t* button = lv_button_create(body_);
            lv_obj_set_width(button, LV_PCT(100));
            lv_obj_set_height(button, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_ver(button, 14, 0);
            lv_obj_set_style_radius(button, 10, 0);
            lv_obj_set_style_bg_color(button, AccentColor(), 0);
            lv_obj_add_event_cb(button, OnChoiceClicked, LV_EVENT_CLICKED, this);
            lv_obj_t* label = lv_label_create(button);
            lv_label_set_text(label, option.c_str());
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(label, LV_PCT(100));
            lv_obj_center(label);
        }
    }

    bool got = xSemaphoreTake(choice_sem_, pdMS_TO_TICKS(timeout_s * 1000)) == pdTRUE;
    int selected;
    {
        DisplayLockGuard lock(display_);
        selected = choice_selected_;
        choice_pending_ = false;
        CloseRoot();
    }

    if (!got || selected < 0) {
        httpd_resp_set_status(req, !got ? "408 Request Timeout" : "410 Gone");
        httpd_resp_set_type(req, "application/json");
        const char* reason = !got ? "{\"error\":\"timeout\"}\n" : "{\"error\":\"dismissed\"}\n";
        return httpd_resp_send(req, reason, HTTPD_RESP_USE_STRLEN);
    }

    cJSON* answer = cJSON_CreateObject();
    cJSON_AddNumberToObject(answer, "selected", selected);
    cJSON_AddStringToObject(answer, "option", choice_options_[selected].c_str());
    char* answer_str = cJSON_PrintUnformatted(answer);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, answer_str, HTTPD_RESP_USE_STRLEN);
    cJSON_free(answer_str);
    cJSON_Delete(answer);
    return err;
}

esp_err_t PushPanel::HandleClose(httpd_req_t* req) {
    {
        DisplayLockGuard lock(display_);
        if (choice_pending_) {
            choice_selected_ = -1;
            xSemaphoreGive(choice_sem_);
        } else {
            CloseRoot();
        }
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK\n", HTTPD_RESP_USE_STRLEN);
}

void PushPanel::OnChoiceClicked(lv_event_t* event) {
    auto* self = static_cast<PushPanel*>(lv_event_get_user_data(event));
    if (self == nullptr || !self->choice_pending_) {
        return;
    }
    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    self->choice_selected_ = static_cast<int>(lv_obj_get_index(button));
    xSemaphoreGive(self->choice_sem_);
}

void PushPanel::OnCloseClicked(lv_event_t* event) {
    auto* self = static_cast<PushPanel*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->DismissFromUi();
    }
}

void PushPanel::OnBackdropClicked(lv_event_t* event) {
    auto* self = static_cast<PushPanel*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->DismissFromUi();
    }
}

void PushPanel::OpenRoot(const char* title) {
    lv_obj_t* screen = lv_screen_active();
    if (root_ != nullptr) {
        // Reuse the overlay, replace the content.
        lv_obj_clean(body_);
        int32_t inset = bottom_inset_ ? bottom_inset_() : 0;
        lv_obj_align(root_, LV_ALIGN_BOTTOM_MID, 0, -(8 + inset));
        lv_obj_set_style_max_height(root_, MaxCardHeight(), 0);
        lv_obj_set_style_max_height(body_, MaxCardHeight() - kHeaderHeight, 0);
        lv_obj_t* header = lv_obj_get_child(root_, 0);
        lv_obj_t* title_label = lv_obj_get_child(header, 0);
        lv_label_set_text(title_label, title);
        open_state_ = static_cast<int>(Application::GetInstance().GetDeviceState());
        lv_obj_move_foreground(backdrop_);
        lv_obj_move_foreground(root_);
        return;
    }

    // Tap-outside-to-close layer under the card.
    backdrop_ = lv_obj_create(screen);
    lv_obj_remove_style_all(backdrop_);
    lv_obj_set_size(backdrop_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(backdrop_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(backdrop_, OnBackdropClicked, LV_EVENT_CLICKED, this);
    lv_obj_move_foreground(backdrop_);

    // Floating bottom card parented to the screen so labels inherit the theme
    // text font (which can be reloaded at runtime and must never be cached).
    // It covers only the lower part of the screen, leaving the status bar and
    // the assistant's face visible above it, like a chat window.
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(96), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(root_, MaxCardHeight(), 0);
    int32_t inset = bottom_inset_ ? bottom_inset_() : 0;
    lv_obj_align(root_, LV_ALIGN_BOTTOM_MID, 0, -(8 + inset));
    lv_obj_set_style_bg_color(root_, lv_obj_get_style_bg_color(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root_, 16, 0);
    lv_obj_set_style_border_width(root_, 1, 0);
    lv_obj_set_style_border_color(root_, AccentColor(), 0);
    lv_obj_set_style_border_opa(root_, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(root_, 24, 0);
    lv_obj_set_style_shadow_opa(root_, LV_OPA_20, 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_move_foreground(root_);

    lv_obj_t* header = lv_obj_create(root_);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), kHeaderHeight);
    lv_obj_set_style_pad_hor(header, 16, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_black(), 0);
    lv_obj_set_style_border_opa(header, LV_OPA_10, 0);

    lv_obj_t* title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(title_label, 1);
    lv_obj_set_style_text_color(title_label, AccentTextColor(), 0);

    lv_obj_t* close_button = lv_label_create(header);
    lv_label_set_text(close_button, LV_SYMBOL_CLOSE);
    lv_obj_add_flag(close_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(close_button, 16);
    lv_obj_add_event_cb(close_button, OnCloseClicked, LV_EVENT_CLICKED, this);

    // The panel dismisses itself when the device state changes after it was
    // opened (e.g. a conversation starts or ends). Polled at 500 ms so no
    // hook into the shared application code is needed.
    open_state_ = static_cast<int>(Application::GetInstance().GetDeviceState());
    if (state_timer_ == nullptr) {
        state_timer_ = lv_timer_create(
            [](lv_timer_t* timer) {
                auto* self = static_cast<PushPanel*>(lv_timer_get_user_data(timer));
                int current = static_cast<int>(Application::GetInstance().GetDeviceState());
                if (current != self->open_state_) {
                    self->DismissFromUi();
                }
            },
            500, this);
    }

    body_ = lv_obj_create(root_);
    lv_obj_remove_style_all(body_);
    lv_obj_set_width(body_, LV_PCT(100));
    lv_obj_set_height(body_, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(body_, MaxCardHeight() - kHeaderHeight, 0);
    lv_obj_set_style_pad_hor(body_, 16, 0);
    lv_obj_set_style_pad_bottom(body_, 16, 0);
    lv_obj_set_style_pad_row(body_, 10, 0);
    lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body_, LV_DIR_VER);
}

void PushPanel::CloseRoot() {
    ArmTtl(0);
    if (state_timer_ != nullptr) {
        lv_timer_delete(state_timer_);
        state_timer_ = nullptr;
    }
    if (backdrop_ != nullptr) {
        lv_obj_delete(backdrop_);
        backdrop_ = nullptr;
    }
    if (root_ == nullptr) {
        return;
    }
    lv_obj_delete(root_);
    root_ = nullptr;
    body_ = nullptr;
}

void PushPanel::DismissFromUi() {
    if (choice_pending_) {
        choice_selected_ = -1;
        xSemaphoreGive(choice_sem_);
        // The waiting HTTP worker closes the panel.
        return;
    }
    CloseRoot();
}

void PushPanel::ArmTtl(int ttl_s) {
    if (ttl_timer_ != nullptr) {
        lv_timer_delete(ttl_timer_);
        ttl_timer_ = nullptr;
    }
    if (ttl_s <= 0) {
        return;
    }
    ttl_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<PushPanel*>(lv_timer_get_user_data(timer));
            self->ttl_timer_ = nullptr;  // one-shot: deleted right after
            self->CloseRoot();
        },
        ttl_s * 1000, this);
    lv_timer_set_repeat_count(ttl_timer_, 1);
}

void PushPanel::AddTextBlock(const std::string& text, int heading_level) {
    if (text.empty()) {
        return;
    }
    lv_obj_t* label = lv_label_create(body_);
    lv_label_set_text(label, text.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    if (heading_level > 0) {
        lv_obj_set_style_text_font(label, &font_noto_sans_basic_30_4, 0);
        lv_obj_set_style_text_color(label, AccentTextColor(), 0);
        lv_obj_set_style_pad_top(label, heading_level == 1 ? 8 : 4, 0);
    } else if (large_text_) {
        lv_obj_set_style_text_font(label, &font_noto_sans_basic_30_4, 0);
    }
}

void PushPanel::RenderTable(const std::vector<std::string>& lines) {
    std::vector<std::vector<std::string>> rows;
    size_t columns = 0;
    for (const auto& line : lines) {
        std::vector<std::string> cells = SplitTableRow(line);
        if (cells.empty() || IsSeparatorRow(cells)) {
            continue;
        }
        columns = std::max(columns, cells.size());
        rows.push_back(std::move(cells));
    }
    if (rows.empty() || columns == 0) {
        return;
    }

    lv_obj_t* table = lv_table_create(body_);
    lv_table_set_column_count(table, columns);
    lv_table_set_row_count(table, rows.size());
    // Fit the columns to the current (rotation-aware) screen width.
    int32_t width = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t column_width = std::max<int32_t>(80, (width - 40) / static_cast<int32_t>(columns));
    for (size_t c = 0; c < columns; ++c) {
        lv_table_set_column_width(table, c, column_width);
    }
    for (size_t r = 0; r < rows.size(); ++r) {
        for (size_t c = 0; c < rows[r].size(); ++c) {
            lv_table_set_cell_value(table, r, c, StripInline(rows[r][c]).c_str());
        }
    }
    lv_obj_add_flag(table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(table, TableDrawTask, LV_EVENT_DRAW_TASK_ADDED, nullptr);
    lv_obj_set_width(table, LV_PCT(100));
    lv_obj_set_height(table, LV_SIZE_CONTENT);
}

void PushPanel::RenderMarkdown(const std::string& text) {
    std::vector<std::string> table_lines;
    std::string paragraph;

    auto flush_paragraph = [&]() {
        if (!paragraph.empty()) {
            AddTextBlock(StripInline(paragraph), 0);
            paragraph.clear();
        }
    };
    auto flush_table = [&]() {
        if (!table_lines.empty()) {
            RenderTable(table_lines);
            table_lines.clear();
        }
    };

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Tables are runs of consecutive '|' lines.
        std::string trimmed = line;
        size_t b = trimmed.find_first_not_of(" \t");
        trimmed = b == std::string::npos ? "" : trimmed.substr(b);
        if (!trimmed.empty() && trimmed[0] == '|') {
            flush_paragraph();
            table_lines.push_back(trimmed);
            if (pos > text.size()) {
                break;
            }
            continue;
        }
        flush_table();

        if (trimmed.empty()) {
            flush_paragraph();
        } else if (trimmed[0] == '#') {
            flush_paragraph();
            int level = 0;
            while (level < static_cast<int>(trimmed.size()) && trimmed[level] == '#') {
                ++level;
            }
            std::string heading = trimmed.substr(level);
            size_t hb = heading.find_first_not_of(" \t");
            heading = hb == std::string::npos ? "" : heading.substr(hb);
            AddTextBlock(StripInline(heading), level);
        } else if (trimmed.size() >= 2 && (trimmed[0] == '-' || trimmed[0] == '*') &&
                   trimmed[1] == ' ') {
            flush_paragraph();
            AddTextBlock("• " + StripInline(trimmed.substr(2)), 0);
        } else {
            if (!paragraph.empty()) {
                paragraph += " ";
            }
            paragraph += trimmed;
        }

        if (pos > text.size()) {
            break;
        }
    }
    flush_paragraph();
    flush_table();
}
