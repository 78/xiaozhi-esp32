#include "local_control_panel.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>

#include "settings.h"

namespace {

constexpr size_t kSaltSize = 16;
constexpr size_t kPasswordHashSize = 32;
constexpr int kPbkdf2Iterations = 100'000;
constexpr int64_t kSessionLifetimeMs = 30 * 60 * 1000;
constexpr int64_t kLoginWindowMs = 5 * 60 * 1000;
constexpr int64_t kLoginLockoutMs = 5 * 60 * 1000;
constexpr int kMaxFailedLogins = 5;
constexpr size_t kMaxRequestBody = 1024;
const char* TAG = "LocalControlPanel";

const char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kevin Box 2</title><style>
:root{color-scheme:dark;--bg:#0b0f14;--card:#141b24;--line:#253244;--text:#eef4ff;--muted:#91a0b5;--accent:#68d8a5;--danger:#ff7d7d}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#172233 0,var(--bg) 46%);color:var(--text);font:15px/1.5 system-ui,sans-serif}.wrap{max-width:760px;margin:auto;padding:28px 18px 48px}header{display:flex;align-items:end;justify-content:space-between;margin-bottom:18px}h1{font-size:25px;margin:0}small,.muted{color:var(--muted)}.card{background:color-mix(in srgb,var(--card) 92%,transparent);border:1px solid var(--line);border-radius:16px;padding:16px;margin:12px 0;box-shadow:0 18px 45px #0004}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}.metric{padding:12px;border-radius:12px;background:#0e141d}.metric b{display:block;font-size:18px;margin-top:4px}input,select,button{width:100%;border:1px solid var(--line);border-radius:10px;padding:11px 12px;margin:6px 0;background:#0d131b;color:var(--text)}button{background:var(--accent);color:#062417;border:0;font-weight:700;cursor:pointer}.secondary{background:#263345;color:var(--text)}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}.hidden{display:none!important}#message{min-height:24px;color:var(--accent)}.danger{color:var(--danger)}code{color:#b6c7dc}@media(max-width:520px){.row{grid-template-columns:1fr}}
</style></head><body><main class="wrap"><header><div><small>LOCAL DEVICE CONTROL</small><h1>Kevin Box 2</h1></div><small id="exposure"></small></header><div id="message"></div>
<section id="auth" class="card"><h2 id="authTitle">登录</h2><input id="password" type="password" minlength="10" maxlength="64" autocomplete="current-password" placeholder="管理密码（至少 10 位）"><button id="authButton">登录</button><small id="authHint">会话有效期 30 分钟。</small></section>
<div id="panel" class="hidden"><section class="card"><div class="grid" id="metrics"></div></section>
<section class="card"><h2>网络模式</h2><div class="row"><select id="mode"><option value="auto">自动</option><option value="wifi">Wi-Fi</option><option value="cellular">4G</option></select><button id="setMode">应用</button></div></section>
<section class="card"><h2>设备设置</h2><label><input id="lanEnabled" type="checkbox" style="width:auto"> 在 Wi-Fi 局域网启用面板</label><input id="volume" type="number" min="0" max="100" placeholder="扬声器音量 0–100"><input id="ssid" maxlength="32" placeholder="新的 Wi-Fi 名称（可选）"><input id="wifiPassword" type="password" maxlength="63" placeholder="新的 Wi-Fi 密码（可选）"><label><input id="restart" type="checkbox" style="width:auto"> 保存后重启</label><button id="save">保存设置</button></section>
<section class="card"><h2>修改管理密码</h2><input id="newPassword" type="password" minlength="10" maxlength="64" placeholder="新密码（至少 10 位）"><button id="changePassword" class="secondary">修改密码</button></section></div></main>
<script>
let csrf='',needsSetup=false;const $=id=>document.getElementById(id),msg=(t,bad=false)=>{$('message').textContent=t;$('message').className=bad?'danger':''};
async function api(path,opt={}){opt.credentials='same-origin';opt.headers={...(opt.headers||{}),'Content-Type':'application/json'};if(csrf)opt.headers['X-CSRF-Token']=csrf;const r=await fetch(path,opt);let j={};try{j=await r.json()}catch{}if(!r.ok)throw new Error(j.error||('HTTP '+r.status));return j}
function render(s){$('exposure').textContent=s.maintenance_mode?'维护热点':'Wi-Fi 局域网';$('mode').value=s.network?.mode||'auto';$('lanEnabled').checked=!!s.lan_enabled;const values=[['当前网络',s.network?.active||'none'],['候选网络',s.network?.candidate||'none'],['Wi-Fi',`${s.network?.wifi_health||'down'} · ${s.wifi_rssi??0} dBm`],['4G',`${s.network?.cellular_health||'down'} · CSQ ${s.cellular_csq??-1}`],['电池',`${s.battery?.level??0}%`],['唤醒',s.wake_engine||'unknown'],['运行时间',`${s.uptime_seconds||0}s`],['可用内存',`${s.free_heap||0} B`],['最近切换',s.network?.last_switch_reason||'none'],['设备状态',s.device_state||'unknown'],['固件',s.firmware?.version||'unknown'],['最近错误',s.recent_error||'无']];$('metrics').innerHTML=values.map(v=>`<div class="metric"><small>${v[0]}</small><b>${String(v[1]).replace(/[<>&]/g,'')}</b></div>`).join('');$('panel').classList.remove('hidden');$('auth').classList.add('hidden')}
async function refresh(){try{const s=await api('/api/status');if(!s.password_configured){needsSetup=true;$('authTitle').textContent='首次设置管理密码';$('authButton').textContent='设置密码';$('authHint').textContent='首次设置只能在本次维护热点中完成。';return}render(s)}catch(e){$('auth').classList.remove('hidden');msg(e.message,e.message!=='Unauthorized')}}
$('authButton').onclick=async()=>{try{const p=$('password').value;if(needsSetup){const r=await api('/api/admin/password',{method:'POST',body:JSON.stringify({new_password:p})});csrf=r.csrf}else{const r=await api('/api/session',{method:'POST',body:JSON.stringify({password:p})});csrf=r.csrf}$('password').value='';msg('已登录');await refresh()}catch(e){msg(e.message,true)}};
$('setMode').onclick=async()=>{try{await api('/api/network/mode',{method:'POST',body:JSON.stringify({mode:$('mode').value})});msg('网络模式已更新')}catch(e){msg(e.message,true)}};
$('save').onclick=async()=>{const b={lan_enabled:$('lanEnabled').checked};if($('volume').value!=='')b.speaker_volume=Number($('volume').value);if($('ssid').value){b.wifi_ssid=$('ssid').value;b.wifi_password=$('wifiPassword').value}if($('restart').checked)b.restart=true;try{await api('/api/settings',{method:'POST',body:JSON.stringify(b)});$('wifiPassword').value='';msg(b.restart?'设置已保存，设备即将重启':'设置已保存')}catch(e){msg(e.message,true)}};
$('changePassword').onclick=async()=>{try{const r=await api('/api/admin/password',{method:'POST',body:JSON.stringify({new_password:$('newPassword').value})});csrf=r.csrf;$('newPassword').value='';msg('管理密码已修改')}catch(e){msg(e.message,true)}};refresh();setInterval(()=>{if(!$('panel').classList.contains('hidden'))refresh()},5000);
</script></body></html>)HTML";

int64_t NowMs() {
    return esp_timer_get_time() / 1000;
}

void SetResponseHeaders(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
    httpd_resp_set_hdr(req, "Content-Security-Policy",
                       "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
                       "object-src 'none'; frame-ancestors 'none'");
}

esp_err_t SendJson(httpd_req_t* req, const char* status, cJSON* json) {
    SetResponseHeaders(req);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    char* body = cJSON_PrintUnformatted(json);
    if (body == nullptr) {
        return httpd_resp_send(req, "{\"error\":\"Out of memory\"}", HTTPD_RESP_USE_STRLEN);
    }
    const esp_err_t result = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    cJSON_free(body);
    return result;
}

esp_err_t SendMessage(httpd_req_t* req, const char* status, const char* key,
                      const char* message) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, key, message);
    const esp_err_t result = SendJson(req, status, root);
    cJSON_Delete(root);
    return result;
}

cJSON* ReadJsonBody(httpd_req_t* req) {
    if (req->content_len <= 0 || req->content_len > kMaxRequestBody) {
        return nullptr;
    }
    std::vector<char> body(static_cast<size_t>(req->content_len) + 1, '\0');
    size_t received = 0;
    while (received < static_cast<size_t>(req->content_len)) {
        const int chunk = httpd_req_recv(req, body.data() + received,
                                         static_cast<size_t>(req->content_len) - received);
        if (chunk == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (chunk <= 0) {
            return nullptr;
        }
        received += static_cast<size_t>(chunk);
    }
    return cJSON_ParseWithLength(body.data(), received);
}

void ZeroJsonString(cJSON* item) {
    if (cJSON_IsString(item) && item->valuestring != nullptr) {
        mbedtls_platform_zeroize(item->valuestring, strlen(item->valuestring));
    }
}

std::string RandomHex(size_t bytes) {
    std::vector<uint8_t> random(bytes);
    esp_fill_random(random.data(), random.size());
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(bytes * 2, '0');
    for (size_t i = 0; i < bytes; ++i) {
        result[i * 2] = kHex[random[i] >> 4];
        result[i * 2 + 1] = kHex[random[i] & 0x0f];
    }
    mbedtls_platform_zeroize(random.data(), random.size());
    return result;
}

bool ConstantTimeEqual(const uint8_t* lhs, const uint8_t* rhs, size_t size) {
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) {
        difference |= lhs[i] ^ rhs[i];
    }
    return difference == 0;
}

bool ConstantTimeStringEqual(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return ConstantTimeEqual(reinterpret_cast<const uint8_t*>(lhs.data()),
                             reinterpret_cast<const uint8_t*>(rhs.data()), lhs.size());
}

bool DerivePasswordHash(const std::string& password, const uint8_t* salt, size_t salt_size,
                        int iterations, uint8_t* output, size_t output_size) {
    if (psa_crypto_init() != PSA_SUCCESS) {
        return false;
    }
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    const psa_algorithm_t algorithm = PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256);
    psa_status_t status = psa_key_derivation_setup(&operation, algorithm);
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST,
                                                  static_cast<uint64_t>(iterations));
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt,
                                                salt_size);
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
            reinterpret_cast<const uint8_t*>(password.data()), password.size());
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_output_bytes(&operation, output, output_size);
    }
    psa_key_derivation_abort(&operation);
    return status == PSA_SUCCESS;
}

std::string GetCookieValue(httpd_req_t* req, const char* name) {
    const size_t length = httpd_req_get_hdr_value_len(req, "Cookie");
    if (length == 0 || length > 512) {
        return {};
    }
    std::vector<char> header(length + 1, '\0');
    if (httpd_req_get_hdr_value_str(req, "Cookie", header.data(), header.size()) != ESP_OK) {
        return {};
    }
    const std::string needle = std::string(name) + "=";
    const std::string cookies(header.data());
    size_t position = 0;
    while ((position = cookies.find(needle, position)) != std::string::npos) {
        if (position == 0 || cookies[position - 1] == ' ' || cookies[position - 1] == ';') {
            const size_t start = position + needle.size();
            const size_t end = cookies.find(';', start);
            return cookies.substr(start, end == std::string::npos ? std::string::npos : end - start);
        }
        position += needle.size();
    }
    return {};
}

const char* StatusForAuthFailure() {
    return "401 Unauthorized";
}

}  // namespace

LocalControlPanel::LocalControlPanel(StatusProvider status_provider,
                                     NetworkModeSetter mode_setter,
                                     SettingsUpdater settings_updater)
    : status_provider_(std::move(status_provider)),
      mode_setter_(std::move(mode_setter)),
      settings_updater_(std::move(settings_updater)) {}

LocalControlPanel::~LocalControlPanel() {
    Stop();
}

bool LocalControlPanel::Start(Exposure exposure) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_ != nullptr && exposure_ == exposure) {
            return true;
        }
    }
    Stop();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start local control panel");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server_ = server;
        exposure_ = exposure;
    }
    if (!RegisterHandlers()) {
        Stop();
        return false;
    }
    ESP_LOGI(TAG, "Local control panel started on %s WiFi interface",
             exposure == Exposure::Maintenance ? "maintenance" : "LAN");
    return true;
}

void LocalControlPanel::Stop() {
    httpd_handle_t server = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server = server_;
        server_ = nullptr;
        ClearSessionsLocked();
    }
    if (server != nullptr) {
        httpd_stop(server);
        ESP_LOGI(TAG, "Local control panel stopped");
    }
}

bool LocalControlPanel::IsRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ != nullptr;
}

bool LocalControlPanel::IsMaintenanceMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ != nullptr && exposure_ == Exposure::Maintenance;
}

bool LocalControlPanel::HasAdminPassword() const {
    Settings settings("local_admin");
    const auto salt = settings.GetBlob("salt");
    const auto hash = settings.GetBlob("pwd_hash");
    return salt.size() == kSaltSize && hash.size() == kPasswordHashSize &&
           settings.GetInt("iterations", 0) > 0;
}

void LocalControlPanel::ResetAdminPassword() {
    {
        Settings settings("local_admin", true);
        settings.EraseAll();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    ClearSessionsLocked();
    failed_login_attempts_ = 0;
    failed_login_window_started_ms_ = 0;
    login_locked_until_ms_ = 0;
    ESP_LOGI(TAG, "Local administrator password reset");
}

bool LocalControlPanel::IsLanEnabled() const {
    Settings settings("local_panel");
    return settings.GetBool("lan_enabled", true);
}

bool LocalControlPanel::RegisterHandlers() {
    httpd_handle_t server = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server = server_;
    }
    if (server == nullptr) {
        return false;
    }
    const httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = IndexHandler, .user_ctx = this},
        {.uri = "/api/status", .method = HTTP_GET, .handler = StatusHandler, .user_ctx = this},
        {.uri = "/api/session", .method = HTTP_POST, .handler = SessionHandler, .user_ctx = this},
        {.uri = "/api/network/mode", .method = HTTP_POST, .handler = NetworkModeHandler, .user_ctx = this},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = SettingsHandler, .user_ctx = this},
        {.uri = "/api/admin/password", .method = HTTP_POST, .handler = AdminPasswordHandler, .user_ctx = this},
    };
    for (const auto& handler : handlers) {
        if (httpd_register_uri_handler(server, &handler) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register panel handler: %s", handler.uri);
            return false;
        }
    }
    return true;
}

esp_err_t LocalControlPanel::IndexHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleIndex(req);
}

esp_err_t LocalControlPanel::StatusHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleStatus(req);
}

esp_err_t LocalControlPanel::SessionHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleSession(req);
}

esp_err_t LocalControlPanel::NetworkModeHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleNetworkMode(req);
}

esp_err_t LocalControlPanel::SettingsHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleSettings(req);
}

esp_err_t LocalControlPanel::AdminPasswordHandler(httpd_req_t* req) {
    return static_cast<LocalControlPanel*>(req->user_ctx)->HandleAdminPassword(req);
}

esp_err_t LocalControlPanel::HandleIndex(httpd_req_t* req) {
    SetResponseHeaders(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kIndexHtml, sizeof(kIndexHtml) - 1);
}

esp_err_t LocalControlPanel::HandleStatus(httpd_req_t* req) {
    const bool password_configured = HasAdminPassword();
    if (password_configured) {
        if (!Authenticate(req, false)) {
            return SendMessage(req, StatusForAuthFailure(), "error", "Unauthorized");
        }
    } else if (!IsMaintenanceMode()) {
        return SendMessage(req, "403 Forbidden", "error", "Password setup requires maintenance mode");
    }

    cJSON* root = status_provider_ ? status_provider_() : cJSON_CreateObject();
    if (root == nullptr) {
        root = cJSON_CreateObject();
    }
    cJSON_AddBoolToObject(root, "password_configured", password_configured);
    cJSON_AddBoolToObject(root, "maintenance_mode", IsMaintenanceMode());
    cJSON_AddBoolToObject(root, "lan_enabled", IsLanEnabled());
    const esp_err_t result = SendJson(req, "200 OK", root);
    cJSON_Delete(root);
    return result;
}

esp_err_t LocalControlPanel::HandleSession(httpd_req_t* req) {
    if (!HasAdminPassword()) {
        return SendMessage(req, "428 Precondition Required", "error", "Password is not configured");
    }
    const int64_t now = NowMs();
    if (!LoginAllowed(now)) {
        return SendMessage(req, "429 Too Many Requests", "error", "Too many attempts; try later");
    }

    cJSON* root = ReadJsonBody(req);
    cJSON* password_item = root ? cJSON_GetObjectItemCaseSensitive(root, "password") : nullptr;
    if (!cJSON_IsString(password_item) || password_item->valuestring == nullptr) {
        cJSON_Delete(root);
        return SendMessage(req, "400 Bad Request", "error", "Invalid request");
    }
    std::string password(password_item->valuestring);
    const bool valid = VerifyPassword(password);
    mbedtls_platform_zeroize(password.data(), password.size());
    mbedtls_platform_zeroize(password_item->valuestring,
                             strlen(password_item->valuestring));
    cJSON_Delete(root);
    RecordLoginResult(valid, now);
    if (!valid) {
        return SendMessage(req, StatusForAuthFailure(), "error", "Invalid credentials");
    }

    std::string token;
    std::string csrf;
    if (!CreateSession(token, csrf)) {
        return SendMessage(req, "500 Internal Server Error", "error", "Unable to create session");
    }
    const std::string cookie = "kb_session=" + token +
                               "; Path=/; Max-Age=1800; HttpOnly; SameSite=Strict";
    httpd_resp_set_hdr(req, "Set-Cookie", cookie.c_str());
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "csrf", csrf.c_str());
    const esp_err_t result = SendJson(req, "200 OK", response);
    cJSON_Delete(response);
    return result;
}

esp_err_t LocalControlPanel::HandleNetworkMode(httpd_req_t* req) {
    if (!Authenticate(req, true)) {
        return SendMessage(req, StatusForAuthFailure(), "error", "Unauthorized");
    }
    cJSON* root = ReadJsonBody(req);
    cJSON* mode_item = root ? cJSON_GetObjectItemCaseSensitive(root, "mode") : nullptr;
    NetworkMode mode;
    const bool valid = cJSON_IsString(mode_item) && mode_item->valuestring != nullptr &&
                       ParseNetworkMode(mode_item->valuestring, mode);
    cJSON_Delete(root);
    if (!valid) {
        return SendMessage(req, "400 Bad Request", "error", "Mode must be auto, wifi, or cellular");
    }
    if (!mode_setter_) {
        return SendMessage(req, "409 Conflict", "error", "Network mode change was rejected");
    }

    // A mode change can stop this HTTP server before committing a switch to cellular.
    // Finish the response first so the client receives a deterministic acknowledgement.
    const esp_err_t response = SendMessage(req, "200 OK", "status", "ok");
    if (response == ESP_OK && !mode_setter_(mode)) {
        ESP_LOGW(TAG, "Network mode change was rejected after acknowledgement");
    }
    return response;
}

esp_err_t LocalControlPanel::HandleSettings(httpd_req_t* req) {
    if (!Authenticate(req, true)) {
        return SendMessage(req, StatusForAuthFailure(), "error", "Unauthorized");
    }
    cJSON* root = ReadJsonBody(req);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return SendMessage(req, "400 Bad Request", "error", "Invalid request");
    }

    LocalPanelSettingsUpdate update;
    if (cJSON* item = cJSON_GetObjectItemCaseSensitive(root, "lan_enabled")) {
        if (!cJSON_IsBool(item)) {
            cJSON_Delete(root);
            return SendMessage(req, "400 Bad Request", "error", "lan_enabled must be boolean");
        }
        update.has_lan_enabled = true;
        update.lan_enabled = cJSON_IsTrue(item);
    }
    if (cJSON* item = cJSON_GetObjectItemCaseSensitive(root, "speaker_volume")) {
        if (!cJSON_IsNumber(item) || item->valueint < 0 || item->valueint > 100) {
            cJSON_Delete(root);
            return SendMessage(req, "400 Bad Request", "error", "speaker_volume must be 0 to 100");
        }
        update.has_speaker_volume = true;
        update.speaker_volume = item->valueint;
    }
    cJSON* ssid_item = cJSON_GetObjectItemCaseSensitive(root, "wifi_ssid");
    cJSON* wifi_password_item = cJSON_GetObjectItemCaseSensitive(root, "wifi_password");
    if (ssid_item != nullptr || wifi_password_item != nullptr) {
        if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == nullptr ||
            !cJSON_IsString(wifi_password_item) || wifi_password_item->valuestring == nullptr) {
            cJSON_Delete(root);
            return SendMessage(req, "400 Bad Request", "error", "WiFi name and password must be strings");
        }
        update.wifi_ssid = ssid_item->valuestring;
        update.wifi_password = wifi_password_item->valuestring;
        const size_t password_length = update.wifi_password.size();
        if (update.wifi_ssid.empty() || update.wifi_ssid.size() > 32 ||
            (password_length != 0 && (password_length < 8 || password_length > 63))) {
            mbedtls_platform_zeroize(update.wifi_password.data(), update.wifi_password.size());
            ZeroJsonString(wifi_password_item);
            cJSON_Delete(root);
            return SendMessage(req, "400 Bad Request", "error", "Invalid WiFi credentials");
        }
        update.has_wifi_credentials = true;
    }
    if (cJSON* item = cJSON_GetObjectItemCaseSensitive(root, "restart")) {
        if (!cJSON_IsBool(item)) {
            mbedtls_platform_zeroize(update.wifi_password.data(), update.wifi_password.size());
            ZeroJsonString(wifi_password_item);
            cJSON_Delete(root);
            return SendMessage(req, "400 Bad Request", "error", "restart must be boolean");
        }
        update.restart_requested = cJSON_IsTrue(item);
    }

    std::string error;
    const bool updated = settings_updater_ && settings_updater_(update, error);
    mbedtls_platform_zeroize(update.wifi_password.data(), update.wifi_password.size());
    ZeroJsonString(wifi_password_item);
    cJSON_Delete(root);
    if (!updated) {
        return SendMessage(req, "409 Conflict", "error",
                           error.empty() ? "Settings were rejected" : error.c_str());
    }
    return SendMessage(req, "200 OK", "status", "ok");
}

esp_err_t LocalControlPanel::HandleAdminPassword(httpd_req_t* req) {
    const bool password_configured = HasAdminPassword();
    if (password_configured) {
        if (!Authenticate(req, true)) {
            return SendMessage(req, StatusForAuthFailure(), "error", "Unauthorized");
        }
    } else if (!IsMaintenanceMode()) {
        return SendMessage(req, "403 Forbidden", "error", "First password must be set in maintenance mode");
    }

    cJSON* root = ReadJsonBody(req);
    cJSON* password_item = root ? cJSON_GetObjectItemCaseSensitive(root, "new_password") : nullptr;
    if (!cJSON_IsString(password_item) || password_item->valuestring == nullptr) {
        cJSON_Delete(root);
        return SendMessage(req, "400 Bad Request", "error", "Invalid request");
    }
    std::string password(password_item->valuestring);
    if (password.size() < 10 || password.size() > 64) {
        mbedtls_platform_zeroize(password.data(), password.size());
        ZeroJsonString(password_item);
        cJSON_Delete(root);
        return SendMessage(req, "400 Bad Request", "error", "Password must contain 10 to 64 characters");
    }
    const bool stored = StorePassword(password);
    mbedtls_platform_zeroize(password.data(), password.size());
    mbedtls_platform_zeroize(password_item->valuestring,
                             strlen(password_item->valuestring));
    cJSON_Delete(root);
    if (!stored) {
        return SendMessage(req, "500 Internal Server Error", "error", "Unable to store password");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ClearSessionsLocked();
    }
    std::string token;
    std::string csrf;
    if (!CreateSession(token, csrf)) {
        return SendMessage(req, "500 Internal Server Error", "error", "Unable to create session");
    }
    const std::string cookie = "kb_session=" + token +
                               "; Path=/; Max-Age=1800; HttpOnly; SameSite=Strict";
    httpd_resp_set_hdr(req, "Set-Cookie", cookie.c_str());
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "csrf", csrf.c_str());
    const esp_err_t result = SendJson(req, "200 OK", response);
    cJSON_Delete(response);
    return result;
}

bool LocalControlPanel::Authenticate(httpd_req_t* req, bool require_csrf) {
    const std::string token = GetCookieValue(req, "kb_session");
    if (token.size() != 64) {
        return false;
    }
    std::string csrf;
    if (require_csrf) {
        char header[65] = {};
        if (httpd_req_get_hdr_value_str(req, "X-CSRF-Token", header, sizeof(header)) != ESP_OK) {
            return false;
        }
        csrf = header;
    }

    const int64_t now = NowMs();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& session : sessions_) {
        if (session.expires_at_ms <= now) {
            session = {};
            continue;
        }
        if (ConstantTimeStringEqual(session.token, token) &&
            (!require_csrf || ConstantTimeStringEqual(session.csrf, csrf))) {
            return true;
        }
    }
    return false;
}

bool LocalControlPanel::CreateSession(std::string& token, std::string& csrf) {
    token = RandomHex(32);
    csrf = RandomHex(16);
    if (token.size() != 64 || csrf.size() != 32) {
        return false;
    }
    const int64_t now = NowMs();
    std::lock_guard<std::mutex> lock(mutex_);
    Session* slot = &sessions_[0];
    for (auto& session : sessions_) {
        if (session.expires_at_ms <= now) {
            slot = &session;
            break;
        }
        if (session.expires_at_ms < slot->expires_at_ms) {
            slot = &session;
        }
    }
    *slot = {.token = token, .csrf = csrf, .expires_at_ms = now + kSessionLifetimeMs};
    return true;
}

bool LocalControlPanel::VerifyPassword(const std::string& password) const {
    Settings settings("local_admin");
    const auto salt = settings.GetBlob("salt");
    auto expected = settings.GetBlob("pwd_hash");
    const int iterations = settings.GetInt("iterations", 0);
    if (salt.size() != kSaltSize || expected.size() != kPasswordHashSize || iterations <= 0) {
        return false;
    }
    std::array<uint8_t, kPasswordHashSize> actual{};
    const bool derived = DerivePasswordHash(password, salt.data(), salt.size(), iterations,
                                            actual.data(), actual.size());
    const bool equal = derived && ConstantTimeEqual(actual.data(), expected.data(), actual.size());
    mbedtls_platform_zeroize(actual.data(), actual.size());
    mbedtls_platform_zeroize(expected.data(), expected.size());
    return equal;
}

bool LocalControlPanel::StorePassword(const std::string& password) {
    std::array<uint8_t, kSaltSize> salt{};
    std::array<uint8_t, kPasswordHashSize> hash{};
    esp_fill_random(salt.data(), salt.size());
    if (!DerivePasswordHash(password, salt.data(), salt.size(), kPbkdf2Iterations, hash.data(),
                            hash.size())) {
        mbedtls_platform_zeroize(salt.data(), salt.size());
        return false;
    }
    {
        Settings settings("local_admin", true);
        settings.SetBlob("salt", salt.data(), salt.size());
        settings.SetBlob("pwd_hash", hash.data(), hash.size());
        settings.SetInt("iterations", kPbkdf2Iterations);
    }
    mbedtls_platform_zeroize(salt.data(), salt.size());
    mbedtls_platform_zeroize(hash.data(), hash.size());
    return true;
}

bool LocalControlPanel::LoginAllowed(int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (login_locked_until_ms_ > now_ms) {
        return false;
    }
    if (login_locked_until_ms_ != 0 || failed_login_window_started_ms_ == 0 ||
        now_ms - failed_login_window_started_ms_ > kLoginWindowMs) {
        failed_login_attempts_ = 0;
        failed_login_window_started_ms_ = now_ms;
        login_locked_until_ms_ = 0;
    }
    return true;
}

void LocalControlPanel::RecordLoginResult(bool success, int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (success) {
        failed_login_attempts_ = 0;
        failed_login_window_started_ms_ = now_ms;
        login_locked_until_ms_ = 0;
        return;
    }
    if (failed_login_window_started_ms_ == 0 ||
        now_ms - failed_login_window_started_ms_ > kLoginWindowMs) {
        failed_login_attempts_ = 0;
        failed_login_window_started_ms_ = now_ms;
    }
    if (++failed_login_attempts_ >= kMaxFailedLogins) {
        login_locked_until_ms_ = now_ms + kLoginLockoutMs;
    }
}

void LocalControlPanel::ClearSessionsLocked() {
    for (auto& session : sessions_) {
        if (!session.token.empty()) {
            mbedtls_platform_zeroize(session.token.data(), session.token.size());
        }
        if (!session.csrf.empty()) {
            mbedtls_platform_zeroize(session.csrf.data(), session.csrf.size());
        }
        session = {};
    }
}
