# 待机天气仪表盘 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 LCDWiki ES3C28P 板子上，设备 idle 时全屏显示含网络状态、城市、AQI、大时钟、日期星期、温湿度进度条和机器人 GIF 动画的待机仪表盘。

**Architecture:** 新增 `main/weather/`（JSON 解析、HTTP GET、Key 存储、天气服务状态机）与 `main/display/dashboard/`（纯映射函数、LVGL 仪表盘 UI、字体、GIF）。Application 在 idle 状态切换仪表盘；数据由 IP 定位 + 和风天气 API 获取，Key 存 NVS，MCP 工具语音设置。

**Tech Stack:** ESP-IDF v6.0.2（esp_http_client、esp_crt_bundle、NVS）、LVGL 9.5、C++23（gnu++23）、cJSON、lv_font_conv 1.5.3、Pillow。

## Global Constraints

- 构建环境：`source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh`；板子 `CONFIG_BOARD_TYPE_LCDWIKI_ES3C28P=y`
- C++ 方言 gnu++23；`idf.py build` 必须 0 error、0 warning
- API Key 不得写死在代码中；只存 NVS 命名空间 `weather` 键 `qweather_key`
- 计时常量：天气刷新 30 分钟；失败重试 5 分钟；GeoIP 缓存 24 小时；时钟心跳 1 秒
- 温度进度条映射范围 -10°C ~ 40°C，钳制；湿度范围 0-100%
- AQI 分档（国标）：0-50 优 / 51-100 良 / 101-150 轻度 / 151-200 中度 / 201-300 重度 / >300 严重
- TLS 使用 ESP-IDF 公共根证书包（`esp_crt_bundle_attach`），不单独管理证书
- 待机 UI 文案使用中文；所有网络失败不得影响聊天与音频主流程

## File Structure

| 文件 | 职责 |
|---|---|
| `main/weather/weather_parsers.h/.cc` | GeoIP / 和风 now / 和风 air 响应的纯解析函数 |
| `main/weather/tests/*` | 解析器主机端 g++ 测试 |
| `main/weather/http_get.h/.cc` | 通用 HTTP/HTTPS GET（cert bundle，5s 超时） |
| `main/weather/weather_key_store.h/.cc` | NVS 中的和风 Key 读写（基于 Settings） |
| `main/weather/weather_service.h/.cc` | 天气状态机任务、快照、更新回调 |
| `main/display/dashboard/dashboard_mappings.h/.cc` | AQI 分档、温度映射、图标码映射、UTF-8、星期（纯函数） |
| `main/display/dashboard/tests/*` | 映射函数主机端测试 |
| `main/display/dashboard/fonts/*.c` | 72px 数字字体、26/36px 天气符号字体（生成并提交） |
| `main/display/dashboard/assets/neutral.gif` | 64×64 待机机器人 GIF（缩放后提交） |
| `main/display/dashboard/dashboard_ui.h/.cc` | LVGL 仪表盘界面 |
| `main/Kconfig.projbuild` | 新增 `WEATHER_DASHBOARD` 选项 |
| `main/CMakeLists.txt` | 条件编译、INCLUDE_DIRS、EMBED_FILES、PRIV_REQUIRES |
| `main/display/display.h` | ShowDashboard/HideDashboard 默认空实现 |
| `main/display/lcd_display.h/.cc` | 持有 DashboardUI、实现 Show/Hide、订阅天气更新 |
| `main/application.cc` | idle 切换仪表盘；网络事件驱动 WeatherService |
| `main/boards/lcdwiki-es3c28p/lcdwiki-es3c28p.cc` | 注册 set_weather_api_key MCP 工具 |

---

### Task 1: JSON 解析纯函数与主机测试

**Files:**
- Create: `main/weather/weather_parsers.h`
- Create: `main/weather/weather_parsers.cc`
- Create: `main/weather/tests/test_weather_parsers.cc`
- Create: `main/weather/tests/run_host_tests.sh`

**Interfaces:**
- Produces（后续任务使用）:
  - `struct GeoInfo { bool ok; std::string city; double lat; double lon; }`
  - `struct WeatherNowData { bool ok; bool key_invalid; std::string text; std::string icon; int temperature; int humidity; }`
  - `struct AirNowData { bool ok; bool key_invalid; int aqi; std::string category; }`
  - `GeoInfo ParseGeoIpResponse(const std::string& body)`
  - `WeatherNowData ParseWeatherNowResponse(const std::string& body, int http_status)`
  - `AirNowData ParseAirNowResponse(const std::string& body, int http_status)`

- [ ] **Step 1: 写失败的主机测试**

创建 `main/weather/tests/test_weather_parsers.cc`：

```cpp
#include <cassert>
#include <string>

#include "weather_parsers.h"

int main() {
    // GeoIP success
    {
        auto geo = ParseGeoIpResponse(
            R"({"status":"success","city":"烟台","lat":37.4638,"lon":121.4479})");
        assert(geo.ok);
        assert(geo.city == "烟台");
        assert(geo.lat > 37.46 && geo.lat < 37.47);
        assert(geo.lon > 121.44 && geo.lon < 121.45);
    }

    // GeoIP failure status
    {
        auto geo = ParseGeoIpResponse(R"({"status":"fail","message":"private range"})");
        assert(!geo.ok);
    }

    // GeoIP missing city
    {
        auto geo = ParseGeoIpResponse(
            R"({"status":"success","lat":37.4638,"lon":121.4479})");
        assert(!geo.ok);
    }

    // GeoIP malformed body
    {
        auto geo = ParseGeoIpResponse("not a json");
        assert(!geo.ok);
    }

    // QWeather now success
    {
        std::string body = R"({"code":"200","now":{"obsTime":"2026-09-23T19:14Z",)"
                           R"("temp":"17","feelsLike":"16","icon":"104","text":"阴",)"
                           R"("wind360":"180","windDir":"南","windScale":"3","windSpeed":"15",)"
                           R"("humidity":"48","precip":"0.0","pressure":"1013","vis":"24",)"
                           R"("cloud":"90","dew":"10"}})";
        auto w = ParseWeatherNowResponse(body, 200);
        assert(w.ok);
        assert(!w.key_invalid);
        assert(w.text == "阴");
        assert(w.icon == "104");
        assert(w.temperature == 17);
        assert(w.humidity == 48);
    }

    // QWeather now business error code
    {
        auto w = ParseWeatherNowResponse(R"({"code":"401"})", 200);
        assert(!w.ok);
        assert(w.key_invalid);
    }

    // QWeather now HTTP 401 with empty body
    {
        auto w = ParseWeatherNowResponse("", 401);
        assert(!w.ok);
        assert(w.key_invalid);
    }

    // QWeather now HTTP 403 with empty body
    {
        auto w = ParseWeatherNowResponse("", 403);
        assert(!w.ok);
        assert(w.key_invalid);
    }

    // QWeather now missing payload fields
    {
        auto w = ParseWeatherNowResponse(R"({"code":"200","now":{}})", 200);
        assert(!w.ok);
        assert(!w.key_invalid);
    }

    // QWeather now malformed
    {
        auto w = ParseWeatherNowResponse("oops", 200);
        assert(!w.ok);
        assert(!w.key_invalid);
    }

    // QWeather air success
    {
        std::string body = R"({"code":"200","now":{"aqi":"57","level":"2",)"
                           R"("category":"良","pm10":"60","pm2p5":"30"}})";
        auto a = ParseAirNowResponse(body, 200);
        assert(a.ok);
        assert(a.aqi == 57);
        assert(a.category == "良");
    }

    // QWeather air invalid key
    {
        auto a = ParseAirNowResponse(R"({"code":"403"})", 200);
        assert(!a.ok);
        assert(a.key_invalid);
    }

    // QWeather air missing field
    {
        auto a = ParseAirNowResponse(R"({"code":"200","now":{}})", 200);
        assert(!a.ok);
    }

    // QWeather air aqi present but category missing
    {
        auto a = ParseAirNowResponse(R"({"code":"200","now":{"aqi":"57"}})", 200);
        assert(!a.ok);
    }

    // QWeather air non-numeric aqi
    {
        auto a = ParseAirNowResponse(
            R"({"code":"200","now":{"aqi":"N/A","category":"良"}})", 200);
        assert(!a.ok);
    }

    return 0;
}
```

- [ ] **Step 2: 运行测试，确认失败**

创建运行脚本 `main/weather/tests/run_host_tests.sh`：

```bash
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../.." && pwd)"
CJSON="$ROOT/managed_components/espressif__cjson/cJSON"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++23 -Wall -Wextra \
    "$DIR/test_weather_parsers.cc" \
    "$ROOT/main/weather/weather_parsers.cc" \
    -I"$CJSON" -I"$ROOT/main/weather" \
    -o "$TMP/test"
"$TMP/test"
echo "weather parser host tests passed"
```

```bash
chmod +x main/weather/tests/run_host_tests.sh
bash main/weather/tests/run_host_tests.sh
```

Expected: FAIL（`weather_parsers.cc` 不存在或 Parse 函数未定义）。

- [ ] **Step 3: 写最小实现**

创建 `main/weather/weather_parsers.h`：

```cpp
#ifndef WEATHER_PARSERS_H
#define WEATHER_PARSERS_H

#include <string>

struct GeoInfo {
    bool ok = false;
    std::string city;
    double lat = 0;
    double lon = 0;
};

struct WeatherNowData {
    bool ok = false;
    bool key_invalid = false;
    std::string text;
    std::string icon;
    int temperature = 0;
    int humidity = 0;
};

struct AirNowData {
    bool ok = false;
    bool key_invalid = false;
    int aqi = -1;
    std::string category;
};

GeoInfo ParseGeoIpResponse(const std::string& body);
WeatherNowData ParseWeatherNowResponse(const std::string& body, int http_status);
AirNowData ParseAirNowResponse(const std::string& body, int http_status);

#endif  // WEATHER_PARSERS_H
```

创建 `main/weather/weather_parsers.cc`：

```cpp
#include "weather_parsers.h"

#include "cjson_utils.h"

namespace {
std::string GetStringField(cJSON* obj, const char* key) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return field != nullptr && cJSON_IsString(field) ? std::string(field->valuestring)
                                                     : std::string();
}

bool TryGetIntField(cJSON* obj, const char* key, int& out) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    if (field == nullptr) {
        return false;
    }
    if (cJSON_IsNumber(field)) {
        out = field->valueint;
        return true;
    }
    if (cJSON_IsString(field)) {
        try {
            out = std::stoi(field->valuestring);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool IsKeyInvalid(const CJsonUniquePtr& root, int http_status) {
    if (http_status == 401 || http_status == 403) {
        return true;
    }
    cJSON* code = cJSON_GetObjectItem(root.get(), "code");
    if (code != nullptr && cJSON_IsString(code)) {
        std::string code_str = code->valuestring;
        return code_str == "401" || code_str == "403";
    }
    return false;
}
}  // namespace

GeoInfo ParseGeoIpResponse(const std::string& body) {
    GeoInfo geo;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        return geo;
    }
    if (GetStringField(root.get(), "status") != "success") {
        return geo;
    }
    cJSON* city = cJSON_GetObjectItem(root.get(), "city");
    cJSON* lat = cJSON_GetObjectItem(root.get(), "lat");
    cJSON* lon = cJSON_GetObjectItem(root.get(), "lon");
    if (city == nullptr || !cJSON_IsString(city) || lat == nullptr || lon == nullptr ||
        !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        return geo;
    }
    geo.ok = true;
    geo.city = city->valuestring;
    geo.lat = lat->valuedouble;
    geo.lon = lon->valuedouble;
    return geo;
}

WeatherNowData ParseWeatherNowResponse(const std::string& body, int http_status) {
    WeatherNowData data;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        data.key_invalid = (http_status == 401 || http_status == 403);
        return data;
    }
    if (IsKeyInvalid(root, http_status)) {
        data.key_invalid = true;
        return data;
    }
    if (GetStringField(root.get(), "code") != "200") {
        return data;
    }
    cJSON* now = cJSON_GetObjectItem(root.get(), "now");
    if (now == nullptr || !cJSON_IsObject(now)) {
        return data;
    }
    data.text = GetStringField(now, "text");
    data.icon = GetStringField(now, "icon");
    if (data.text.empty() || data.icon.empty() ||
        !TryGetIntField(now, "temp", data.temperature) ||
        !TryGetIntField(now, "humidity", data.humidity)) {
        return data;  // Missing/incomplete payload -> treat as request failure
    }
    data.ok = true;
    return data;
}

AirNowData ParseAirNowResponse(const std::string& body, int http_status) {
    AirNowData data;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        data.key_invalid = (http_status == 401 || http_status == 403);
        return data;
    }
    if (IsKeyInvalid(root, http_status)) {
        data.key_invalid = true;
        return data;
    }
    if (GetStringField(root.get(), "code") != "200") {
        return data;
    }
    cJSON* now = cJSON_GetObjectItem(root.get(), "now");
    if (now == nullptr || !cJSON_IsObject(now)) {
        return data;
    }
    cJSON* aqi = cJSON_GetObjectItem(now, "aqi");
    if (aqi == nullptr) {
        return data;
    }
    data.category = GetStringField(now, "category");
    if (data.category.empty()) {
        return data;
    }
    if (cJSON_IsNumber(aqi)) {
        data.aqi = aqi->valueint;
    } else if (cJSON_IsString(aqi)) {
        try {
            data.aqi = std::stoi(aqi->valuestring);
        } catch (...) {
            return data;
        }
    } else {
        return data;
    }
    data.ok = true;
    return data;
}
```

- [ ] **Step 4: 运行测试，确认通过**

```bash
bash main/weather/tests/run_host_tests.sh
```

Expected: 输出 `weather parser host tests passed`。

- [ ] **Step 5: 提交**

```bash
git add main/weather/weather_parsers.h main/weather/weather_parsers.cc \
    main/weather/tests/test_weather_parsers.cc main/weather/tests/run_host_tests.sh
git commit -m "feat(weather): add GeoIP and QWeather response parsers with host tests"
```

---

### Task 2: Kconfig/CMake 脚手架 + HTTP GET + Key 存储

**Files:**
- Modify: `main/Kconfig.projbuild`（文件末尾追加）
- Modify: `main/CMakeLists.txt`（条件块加在 `idf_component_register` 之前；EMBED_FILES 与 PRIV_REQUIRES）
- Create: `main/weather/http_get.h`
- Create: `main/weather/http_get.cc`
- Create: `main/weather/weather_key_store.h`
- Create: `main/weather/weather_key_store.cc`

**Interfaces:**
- Consumes: Task 1 的解析器源文件（加入 SOURCES 参与固件编译）
- Produces:
  - `struct HttpResult { int status; std::string body; }`
  - `HttpResult HttpGet(const std::string& url, int timeout_ms = 5000)`
  - `class WeatherKeyStore { std::string GetKey() const; void SetKey(const std::string&); }`
  - Kconfig 符号 `CONFIG_WEATHER_DASHBOARD`

- [ ] **Step 1: 添加 Kconfig 选项**

在 `main/Kconfig.projbuild` 文件末尾追加：

```kconfig

config WEATHER_DASHBOARD
    bool "Standby weather dashboard (待机天气仪表盘)"
    depends on IDF_TARGET_ESP32S3
    default y if BOARD_TYPE_LCDWIKI_ES3C28P
    default n
```

- [ ] **Step 2: 添加 CMake 条件块**

在 `main/CMakeLists.txt` 的 `idf_component_register(`（约 :1096）**之前**插入：

```cmake
if(CONFIG_WEATHER_DASHBOARD)
    list(APPEND SOURCES
        "weather/weather_parsers.cc"
        "weather/http_get.cc"
        "weather/weather_key_store.cc"
    )
    list(APPEND INCLUDE_DIRS "weather" "display/dashboard")
    set(DASHBOARD_EMBED
        "${CMAKE_CURRENT_SOURCE_DIR}/display/dashboard/assets/neutral.gif")
endif()
```

把 `idf_component_register` 的 EMBED_FILES 行改为：

```cmake
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS} ${DASHBOARD_EMBED}
```

在 `idf_component_register` 的 `PRIV_REQUIRES` 列表中（`fatfs` 附近）增加两行：

```cmake
                        fatfs
                        esp_http_client
                        espressif__cjson
                        xiaozhi-fonts
```

（`xiaozhi-fonts` 已存在，不要重复添加。）

- [ ] **Step 3: 写 HTTP GET 实现**

创建 `main/weather/http_get.h`：

```cpp
#ifndef HTTP_GET_H
#define HTTP_GET_H

#include <string>

struct HttpResult {
    int status = 0;  // HTTP status code; 0 on transport failure
    std::string body;
};

HttpResult HttpGet(const std::string& url, int timeout_ms = 5000);

#endif  // HTTP_GET_H
```

创建 `main/weather/http_get.cc`：

```cpp
#include "http_get.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

#define TAG "HttpGet"

namespace {
constexpr size_t kMaxBodySize = 16 * 1024;

// Replace the value of any "key=" query parameter so API keys never hit logs.
std::string RedactUrl(const std::string& url) {
    size_t pos = 0;
    while (true) {
        pos = url.find("key=", pos);
        if (pos == std::string::npos) {
            return url;
        }
        // Only match a real query parameter: start of URL, or right after ?/&.
        if (pos == 0 || url[pos - 1] == '?' || url[pos - 1] == '&') {
            break;
        }
        pos += 4;
    }
    size_t end = pos + 4;
    while (end < url.size() && url[end] != '&') {
        ++end;
    }
    return url.substr(0, pos) + "key=***" + url.substr(end);
}
}  // namespace

HttpResult HttpGet(const std::string& url, int timeout_ms) {
    HttpResult result;
    std::string redacted = RedactUrl(url);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = timeout_ms;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize client for %s", redacted.c_str());
        return result;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Open failed for %s: %s", redacted.c_str(), esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return result;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGW(TAG, "Fetch headers failed for %s", redacted.c_str());
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return result;
    }
    result.status = esp_http_client_get_status_code(client);

    char buffer[1024];
    int read_len;
    while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        if (result.body.size() + static_cast<size_t>(read_len) > kMaxBodySize) {
            ESP_LOGW(TAG, "Response body exceeds %d bytes for %s",
                     static_cast<int>(kMaxBodySize), redacted.c_str());
            result = HttpResult{};  // Oversized/truncated body -> transport failure
            break;
        }
        result.body.append(buffer, static_cast<size_t>(read_len));
    }
    if (read_len < 0) {
        ESP_LOGW(TAG, "Read error for %s", redacted.c_str());
        result = HttpResult{};
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (result.status != 0) {
        ESP_LOGD(TAG, "GET %s -> %d (%d bytes)", redacted.c_str(), result.status,
                 static_cast<int>(result.body.size()));
    }
    return result;
}
```

- [ ] **Step 4: 写 Key 存储实现**

创建 `main/weather/weather_key_store.h`：

```cpp
#ifndef WEATHER_KEY_STORE_H
#define WEATHER_KEY_STORE_H

#include <string>

class WeatherKeyStore {
public:
    std::string GetKey() const;
    void SetKey(const std::string& key);
};

#endif  // WEATHER_KEY_STORE_H
```

创建 `main/weather/weather_key_store.cc`：

```cpp
#include "weather_key_store.h"

#include "settings.h"

std::string WeatherKeyStore::GetKey() const {
    Settings settings("weather", false);
    return settings.GetString("qweather_key");
}

void WeatherKeyStore::SetKey(const std::string& key) {
    Settings settings("weather", true);
    if (key.empty()) {
        settings.EraseKey("qweather_key");
    } else {
        settings.SetString("qweather_key", key);
    }
}
```

- [ ] **Step 5: 占位 GIF 资源（保证 CMake EMBED 路径存在）**

```bash
mkdir -p main/display/dashboard/assets
cp managed_components/txp666__otto-emoji-gif-component/gifs/neutral.gif \
    main/display/dashboard/assets/neutral.gif
```

（该文件在 Task 5 中会被替换为 64×64 版本。）

- [ ] **Step 6: 固件编译验证**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。（现有 sdkconfig 重新配置时 `CONFIG_WEATHER_DASHBOARD` 取默认 y。可用 `grep CONFIG_WEATHER_DASHBOARD sdkconfig` 确认 `=y`。）

- [ ] **Step 7: 提交**

```bash
git add main/Kconfig.projbuild main/CMakeLists.txt main/weather/http_get.h \
    main/weather/http_get.cc main/weather/weather_key_store.h \
    main/weather/weather_key_store.cc main/display/dashboard/assets/neutral.gif
git commit -m "feat(weather): add Kconfig/CMake scaffold, HTTP GET helper and key store"
```

---

### Task 3: WeatherService 天气状态机

**Files:**
- Create: `main/weather/weather_service.h`
- Create: `main/weather/weather_service.cc`
- Modify: `main/CMakeLists.txt`（SOURCES 增加 `weather/weather_service.cc`）

**Interfaces:**
- Consumes: `HttpGet`、`Parse*`、`WeatherKeyStore`
- Produces（后续任务使用）:
  - `struct WeatherSnapshot { bool valid; bool no_key; bool key_invalid; std::string city; std::string weather_text; std::string icon_code; int temperature; int humidity; int aqi; std::string aqi_category; time_t updated_at; }`
  - `static WeatherService& WeatherService::GetInstance()`
  - `void Start()` / `void OnKeyUpdated()` / `void OnNetworkConnected()` / `void OnNetworkDisconnected()`
  - `WeatherSnapshot GetSnapshot()`
  - `void SetUpdateCallback(std::function<void()>)`

- [ ] **Step 1: 写 WeatherService 头文件**

创建 `main/weather/weather_service.h`：

```cpp
#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>

#include "weather_key_store.h"

struct WeatherSnapshot {
    bool valid = false;
    bool no_key = false;
    bool key_invalid = false;
    std::string city;
    std::string weather_text;
    std::string icon_code;
    int temperature = 0;
    int humidity = 0;
    int aqi = -1;
    std::string aqi_category;
    time_t updated_at = 0;
};

class WeatherService {
public:
    static WeatherService& GetInstance();

    void Start();
    void OnKeyUpdated();
    void OnNetworkConnected();
    void OnNetworkDisconnected();

    WeatherSnapshot GetSnapshot();
    void SetUpdateCallback(std::function<void()> callback);

private:
    WeatherService() = default;
    void TaskLoop();
    void Publish(const WeatherSnapshot& snapshot);

    std::atomic<bool> started_{false};
    std::atomic<bool> network_connected_{false};
    TaskHandle_t task_ = nullptr;
    std::mutex mutex_;
    WeatherSnapshot snapshot_;
    std::function<void()> update_callback_;
    WeatherKeyStore key_store_;
};

#endif  // WEATHER_SERVICE_H
```

- [ ] **Step 2: 写状态机实现**

创建 `main/weather/weather_service.cc`：

```cpp
#include "weather_service.h"

#include "http_get.h"
#include "weather_parsers.h"

#include <esp_log.h>

#include <cstdio>

#define TAG "WeatherService"

namespace {
enum NotifyBits {
    kNotifyKeyUpdated = 1 << 0,
    kNotifyNetwork = 1 << 1,
};
constexpr uint32_t kRefreshIntervalMs = 30 * 60 * 1000;
constexpr uint32_t kRetryIntervalMs = 5 * 60 * 1000;
constexpr time_t kGeoTtlSec = 24 * 60 * 60;
}  // namespace

WeatherService& WeatherService::GetInstance() {
    static WeatherService instance;
    return instance;
}

void WeatherService::Start() {
    if (started_.exchange(true)) {
        return;
    }
    xTaskCreate(
        [](void* arg) {
            auto* self = static_cast<WeatherService*>(arg);
            self->TaskLoop();
            self->task_ = nullptr;
            vTaskDelete(nullptr);
        },
        "weather_svc", 4096 * 2, this, 4, &task_);
}

void WeatherService::OnKeyUpdated() {
    if (task_ != nullptr) {
        xTaskNotify(task_, kNotifyKeyUpdated, eSetBits);
    }
}

void WeatherService::OnNetworkConnected() {
    network_connected_ = true;
    if (task_ != nullptr) {
        xTaskNotify(task_, kNotifyNetwork, eSetBits);
    }
}

void WeatherService::OnNetworkDisconnected() { network_connected_ = false; }

WeatherSnapshot WeatherService::GetSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void WeatherService::SetUpdateCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    update_callback_ = std::move(callback);
}

void WeatherService::Publish(const WeatherSnapshot& snapshot) {
    std::function<void()> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = snapshot;
        callback = update_callback_;
    }
    if (callback) {
        callback();
    }
}

void WeatherService::TaskLoop() {
    GeoInfo geo;
    time_t geo_fetched_at = 0;

    auto wait = [](uint32_t ms) {
        uint32_t received = 0;
        xTaskNotifyWait(0, 0xffffffff, &received, pdMS_TO_TICKS(ms));
        return received;
    };

    for (;;) {
        if (!network_connected_) {
            ESP_LOGI(TAG, "Offline, waiting for network");
            wait(kRetryIntervalMs);
            continue;
        }

        std::string key = key_store_.GetKey();
        if (key.empty()) {
            WeatherSnapshot no_key_snapshot;
            no_key_snapshot.no_key = true;
            Publish(no_key_snapshot);  // UI shows "天气未配置"
            wait(kRetryIntervalMs);
            continue;
        }

        time_t now = time(nullptr);
        if (!geo.ok || now - geo_fetched_at > kGeoTtlSec) {
            auto geo_resp = HttpGet(
                "http://ip-api.com/json/?lang=zh-CN&fields=status,city,lat,lon");
            GeoInfo parsed = ParseGeoIpResponse(geo_resp.body);
            if (!parsed.ok) {
                ESP_LOGW(TAG, "GeoIP lookup failed (http %d)", geo_resp.status);
                wait(kRetryIntervalMs);
                continue;
            }
            geo = parsed;
            geo_fetched_at = time(nullptr);
            ESP_LOGI(TAG, "Located in %s (%.4f, %.4f)", geo.city.c_str(), geo.lat, geo.lon);
        }

        char location[64];
        snprintf(location, sizeof(location), "%.4f,%.4f", geo.lon, geo.lat);
        std::string base = "https://devapi.qweather.com/v7/";
        std::string query = std::string("?location=") + location + "&key=" + key;

        auto weather_resp = HttpGet(base + "weather/now" + query);
        WeatherNowData weather =
            ParseWeatherNowResponse(weather_resp.body, weather_resp.status);
        if (!weather.ok) {
            if (weather.key_invalid) {
                WeatherSnapshot invalid;
                invalid.key_invalid = true;
                invalid.city = geo.city;
                Publish(invalid);
            }
            ESP_LOGW(TAG, "Weather request failed (http %d)", weather_resp.status);
            wait(kRetryIntervalMs);
            continue;
        }

        WeatherSnapshot snapshot;
        snapshot.valid = true;
        snapshot.city = geo.city;
        snapshot.weather_text = weather.text;
        snapshot.icon_code = weather.icon;
        snapshot.temperature = weather.temperature;
        snapshot.humidity = weather.humidity;
        snapshot.updated_at = time(nullptr);

        auto air_resp = HttpGet(base + "air/now" + query);
        AirNowData air = ParseAirNowResponse(air_resp.body, air_resp.status);
        if (air.ok) {
            snapshot.aqi = air.aqi;
            snapshot.aqi_category = air.category;
        } else {
            ESP_LOGW(TAG, "Air quality request failed (http %d), showing weather only",
                     air_resp.status);
        }

        Publish(snapshot);
        ESP_LOGI(TAG, "Weather updated: %s, %dC, %d%%, AQI %d", weather.text.c_str(),
                 weather.temperature, weather.humidity, snapshot.aqi);

        wait(kRefreshIntervalMs);
    }
}
```

- [ ] **Step 3: 加入 CMake SOURCES**

把 `main/CMakeLists.txt` 中 `if(CONFIG_WEATHER_DASHBOARD)` 块的 SOURCES 列表改为：

```cmake
    list(APPEND SOURCES
        "weather/weather_parsers.cc"
        "weather/http_get.cc"
        "weather/weather_key_store.cc"
        "weather/weather_service.cc"
    )
```

- [ ] **Step 4: 固件编译验证**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。

- [ ] **Step 5: 提交**

```bash
git add main/weather/weather_service.h main/weather/weather_service.cc main/CMakeLists.txt
git commit -m "feat(weather): add WeatherService state machine for geo locate and refresh"
```

---

### Task 4: 仪表盘纯映射函数与主机测试

**Files:**
- Create: `main/display/dashboard/dashboard_mappings.h`
- Create: `main/display/dashboard/dashboard_mappings.cc`
- Create: `main/display/dashboard/tests/test_dashboard_mappings.cc`
- Create: `main/display/dashboard/tests/run_host_tests.sh`
- Modify: `main/CMakeLists.txt`（SOURCES 增加 `display/dashboard/dashboard_mappings.cc`）

**Interfaces:**
- Produces（Task 6 使用）:
  - `struct AqiLevelInfo { const char* category; uint32_t color_hex; uint32_t text_color_hex; }`
  - `AqiLevelInfo AqiToLevel(int aqi)`
  - `int TempToPercent(int temp)`
  - `uint32_t WeatherIconToCodepoint(const std::string& icon_code)`
  - `uint32_t WeatherIconColor(uint32_t codepoint)`
  - `void CodepointToUtf8(uint32_t codepoint, char out[5])`
  - `const char* WeekdayZh(int weekday)`

- [ ] **Step 1: 写失败的主机测试**

创建 `main/display/dashboard/tests/test_dashboard_mappings.cc`：

```cpp
#include <cassert>
#include <cstring>

#include "dashboard_mappings.h"

int main() {
    // AQI levels and boundaries
    assert(std::string(AqiToLevel(0).category) == "优");
    assert(std::string(AqiToLevel(50).category) == "优");
    assert(std::string(AqiToLevel(51).category) == "良");
    assert(std::string(AqiToLevel(100).category) == "良");
    assert(std::string(AqiToLevel(101).category) == "轻度");
    assert(std::string(AqiToLevel(150).category) == "轻度");
    assert(std::string(AqiToLevel(151).category) == "中度");
    assert(std::string(AqiToLevel(200).category) == "中度");
    assert(std::string(AqiToLevel(201).category) == "重度");
    assert(std::string(AqiToLevel(300).category) == "重度");
    assert(std::string(AqiToLevel(301).category) == "严重");
    assert(AqiToLevel(-1).color_hex == 0x9E9E9E);

    // Temperature mapping -10..40
    assert(TempToPercent(-20) == 0);
    assert(TempToPercent(-10) == 0);
    assert(TempToPercent(15) == 50);
    assert(TempToPercent(40) == 100);
    assert(TempToPercent(50) == 100);

    // QWeather icon code mapping
    assert(WeatherIconToCodepoint("100") == 0x2600);
    assert(WeatherIconToCodepoint("101") == 0x26C5);
    assert(WeatherIconToCodepoint("103") == 0x26C5);
    assert(WeatherIconToCodepoint("104") == 0x2601);
    assert(WeatherIconToCodepoint("302") == 0x26C8);
    assert(WeatherIconToCodepoint("305") == 0x2614);
    assert(WeatherIconToCodepoint("313") == 0x2614);
    assert(WeatherIconToCodepoint("400") == 0x2744);
    assert(WeatherIconToCodepoint("404") == 0x2744);
    assert(WeatherIconToCodepoint("501") == 0x2601);
    assert(WeatherIconToCodepoint("999") == 0x2601);

    // Icon colors
    assert(WeatherIconColor(0x2600) == 0xF9A825);
    assert(WeatherIconColor(0x2601) == 0x607D8B);
    assert(WeatherIconColor(0x2614) == 0x1976D2);
    assert(WeatherIconColor(0x26C8) == 0x6A1B9A);
    assert(WeatherIconColor(0x2744) == 0x4FC3F7);

    // UTF-8 encoding
    char utf8[5];
    CodepointToUtf8('A', utf8);
    assert(std::string(utf8) == "A");
    CodepointToUtf8(0x2601, utf8);
    assert(utf8[0] == static_cast<char>(0xE2) && utf8[1] == static_cast<char>(0x98) &&
           utf8[2] == static_cast<char>(0x81) && utf8[3] == '\0');
    CodepointToUtf8(0x1F321, utf8);
    assert(utf8[0] == static_cast<char>(0xF0) && utf8[3] == static_cast<char>(0xA1));

    // Weekday
    assert(std::string(WeekdayZh(0)) == "周日");
    assert(std::string(WeekdayZh(4)) == "周四");
    assert(std::string(WeekdayZh(6)) == "周六");

    return 0;
}
```

- [ ] **Step 2: 运行测试，确认失败**

创建 `main/display/dashboard/tests/run_host_tests.sh`：

```bash
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++23 -Wall -Wextra \
    "$DIR/test_dashboard_mappings.cc" \
    "$ROOT/main/display/dashboard/dashboard_mappings.cc" \
    -I"$ROOT/main/display/dashboard" \
    -o "$TMP/test"
"$TMP/test"
echo "dashboard mapping host tests passed"
```

```bash
chmod +x main/display/dashboard/tests/run_host_tests.sh
bash main/display/dashboard/tests/run_host_tests.sh
```

Expected: FAIL（映射函数未定义）。

- [ ] **Step 3: 写最小实现**

创建 `main/display/dashboard/dashboard_mappings.h`：

```cpp
#ifndef DASHBOARD_MAPPINGS_H
#define DASHBOARD_MAPPINGS_H

#include <cstdint>
#include <string>

struct AqiLevelInfo {
    const char* category;
    uint32_t color_hex;
    uint32_t text_color_hex;
};

AqiLevelInfo AqiToLevel(int aqi);
int TempToPercent(int temp);
uint32_t WeatherIconToCodepoint(const std::string& icon_code);
uint32_t WeatherIconColor(uint32_t codepoint);
void CodepointToUtf8(uint32_t codepoint, char out[5]);
const char* WeekdayZh(int weekday);

#endif  // DASHBOARD_MAPPINGS_H
```

创建 `main/display/dashboard/dashboard_mappings.cc`：

```cpp
#include "dashboard_mappings.h"

AqiLevelInfo AqiToLevel(int aqi) {
    if (aqi < 0) {
        return {"--", 0x9E9E9E, 0xFFFFFF};
    }
    if (aqi <= 50) {
        return {"优", 0x4CAF50, 0xFFFFFF};
    }
    if (aqi <= 100) {
        return {"良", 0xFDD835, 0x000000};
    }
    if (aqi <= 150) {
        return {"轻度", 0xFB8C00, 0xFFFFFF};
    }
    if (aqi <= 200) {
        return {"中度", 0xE53935, 0xFFFFFF};
    }
    if (aqi <= 300) {
        return {"重度", 0x8E24AA, 0xFFFFFF};
    }
    return {"严重", 0xB71C1C, 0xFFFFFF};
}

int TempToPercent(int temp) {
    if (temp <= -10) {
        return 0;
    }
    if (temp >= 40) {
        return 100;
    }
    return (temp + 10) * 2;  // 50C span over 0-100
}

uint32_t WeatherIconToCodepoint(const std::string& icon_code) {
    int code = 0;
    try {
        code = std::stoi(icon_code);
    } catch (...) {
        return 0x2601;
    }
    if (code == 100) {
        return 0x2600;  // sunny
    }
    if (code >= 101 && code <= 103) {
        return 0x26C5;  // sun behind cloud
    }
    if (code == 104) {
        return 0x2601;  // overcast
    }
    if (code >= 302 && code <= 304) {
        return 0x26C8;  // thunder
    }
    if (code >= 300 && code < 400) {
        return 0x2614;  // rain
    }
    if (code >= 400 && code < 500) {
        return 0x2744;  // snow
    }
    return 0x2601;      // fog/haze/unknown -> cloud
}

uint32_t WeatherIconColor(uint32_t codepoint) {
    switch (codepoint) {
        case 0x2600:
            return 0xF9A825;  // sun amber
        case 0x26C5:
            return 0x90A4AE;  // partly cloudy
        case 0x2614:
            return 0x1976D2;  // rain blue
        case 0x26C8:
            return 0x6A1B9A;  // thunder purple
        case 0x2744:
            return 0x4FC3F7;  // snow light blue
        case 0x2601:
        default:
            return 0x607D8B;  // cloud blue-gray
    }
}

void CodepointToUtf8(uint32_t codepoint, char out[5]) {
    if (codepoint < 0x80) {
        out[0] = static_cast<char>(codepoint);
        out[1] = '\0';
    } else if (codepoint < 0x800) {
        out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
        out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
    } else if (codepoint < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
    } else {
        out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
        out[4] = '\0';
    }
}

const char* WeekdayZh(int weekday) {
    static const char* kWeekdays[] = {"周日", "周一", "周二", "周三",
                                      "周四", "周五", "周六"};
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return kWeekdays[weekday];
}
```

- [ ] **Step 4: 运行测试，确认通过**

```bash
bash main/display/dashboard/tests/run_host_tests.sh
```

Expected: 输出 `dashboard mapping host tests passed`。

- [ ] **Step 5: 加入 CMake 并编译固件**

在 `main/CMakeLists.txt` 的 `if(CONFIG_WEATHER_DASHBOARD)` SOURCES 列表末尾加：

```cmake
        "display/dashboard/dashboard_mappings.cc"
```

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。

- [ ] **Step 6: 提交**

```bash
git add main/display/dashboard/dashboard_mappings.h \
    main/display/dashboard/dashboard_mappings.cc \
    main/display/dashboard/tests/ main/CMakeLists.txt
git commit -m "feat(dashboard): add pure mapping functions for AQI, temp and weather icons"
```

---

### Task 5: 生成字体与缩放 GIF

**Files:**
- Create: `main/display/dashboard/fonts/font_digits_72.c`
- Create: `main/display/dashboard/fonts/font_weather_symbols_36_4.c`
- Create: `main/display/dashboard/fonts/font_weather_symbols_26_4.c`
- Modify: `main/display/dashboard/assets/neutral.gif`（替换为 64×64）

**Interfaces:**
- Produces（Task 6 使用，LVGL 变量名）:
  - `lv_font_t lv_font_digits_72`
  - `lv_font_t font_weather_symbols_36_4`（0x2600, 0x2601, 0x2614, 0x26C5, 0x26C8, 0x2744）
  - `lv_font_t font_weather_symbols_26_4`（0x1F321, 0x1F4A7）

- [ ] **Step 1: 下载 Noto Sans Symbols 2 字体**

```bash
mkdir -p main/display/dashboard/fonts
curl -fL -o main/display/dashboard/fonts/NotoSansSymbols2-Regular.ttf \
    https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssymbols2/NotoSansSymbols2-Regular.ttf
```

Expected: 下载成功（文件约 400KB+）；该 TTF 仅作生成用。

- [ ] **Step 2: 生成 72px 数字字体**

```bash
npx -y lv_font_conv@1.5.3 \
    --font "/System/Library/Fonts/Supplemental/Arial Bold.ttf" \
    --size 72 --bpp 4 --range 0x30-0x3A --no-compress \
    --format lvgl --lv-include lvgl.h --lv-font-name lv_font_digits_72 \
    -o main/display/dashboard/fonts/font_digits_72.c
```

Expected: 命令完成，输出无 `WARN`。

- [ ] **Step 3: 生成 36px 天气图标字体**

```bash
npx -y lv_font_conv@1.5.3 \
    --font main/display/dashboard/fonts/NotoSansSymbols2-Regular.ttf \
    --size 36 --bpp 4 --no-compress \
    --range 0x2600,0x2601,0x2614,0x26C5,0x26C8,0x2744 \
    --format lvgl --lv-include lvgl.h --lv-font-name font_weather_symbols_36_4 \
    -o main/display/dashboard/fonts/font_weather_symbols_36_4.c
```

Expected: 完成且无 `WARN`（6 个字形全部存在）。

- [ ] **Step 4: 生成 26px 环境图标字体**

NotoSansSymbols2 缺 U+1F4A7（U+1F321 有），混合 range 会导致 lv_font_conv 中止；26px 字体改用 Google 单色 NotoEmoji（同为 SIL OFL）：

```bash
npx -y lv_font_conv@1.5.3 \
    --font main/display/dashboard/fonts/NotoEmoji-Regular.ttf \
    --size 26 --bpp 4 --no-compress \
    --range 0x1F321,0x1F4A7 \
    --format lvgl --lv-include lvgl.h --lv-font-name font_weather_symbols_26_4 \
    -o main/display/dashboard/fonts/font_weather_symbols_26_4.c
```

Expected: 完成且无 `WARN`（温度计与水滴字形存在）。NotoEmoji-Regular.ttf 从 https://fonts.gstatic.com/s/notoemoji/ 下载（如 gstatic 不可达，用 jsDelivr 上 google/fonts 镜像 ofl/notoemoji/），作为生成输入一并提交。

- [ ] **Step 5: 缩放待机 GIF 到 64×64**

```bash
python3 - <<'PY'
from PIL import Image, ImageSequence

src_path = "managed_components/txp666__otto-emoji-gif-component/gifs/neutral.gif"
out_path = "main/display/dashboard/assets/neutral.gif"

src = Image.open(src_path)
frames = []
durations = []
for frame in ImageSequence.Iterator(src):
    rgba = frame.convert("RGBA").resize((64, 64), Image.LANCZOS)
    alpha = rgba.getchannel("A")
    paletted = rgba.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
    mask = alpha.point(lambda a: 255 if a <= 128 else 0)
    paletted.paste(255, mask)
    paletted.info["transparency"] = 255
    frames.append(paletted)
    durations.append(frame.info.get("duration", 100))

frames[0].save(out_path, save_all=True, append_images=frames[1:],
               duration=durations, loop=0, disposal=2)
print("resized neutral.gif:", len(frames), "frames")
PY
```

Expected: 输出 `resized neutral.gif: N frames`；用 Python 验证新尺寸：

```bash
python3 -c "
import struct
d=open('main/display/dashboard/assets/neutral.gif','rb').read()
assert struct.unpack('<HH', d[6:10]) == (64,64)
print('64x64 ok')"
```

- [ ] **Step 6: 字体加入 CMake（暂不编译，保证路径登记）**

在 `main/CMakeLists.txt` 的 `if(CONFIG_WEATHER_DASHBOARD)` SOURCES 列表中追加：

```cmake
        "display/dashboard/fonts/font_digits_72.c"
        "display/dashboard/fonts/font_weather_symbols_26_4.c"
        "display/dashboard/fonts/font_weather_symbols_36_4.c"
```

- [ ] **Step 7: 提交**

```bash
git add main/display/dashboard/fonts/font_digits_72.c \
    main/display/dashboard/fonts/font_weather_symbols_36_4.c \
    main/display/dashboard/fonts/font_weather_symbols_26_4.c \
    main/display/dashboard/assets/neutral.gif \
    main/CMakeLists.txt
git commit -m "feat(dashboard): add 72px digit font, weather symbol fonts and 64px idle GIF"
```

---

### Task 6: DashboardUI LVGL 界面

**Files:**
- Create: `main/display/dashboard/dashboard_ui.h`
- Create: `main/display/dashboard/dashboard_ui.cc`
- Modify: `main/CMakeLists.txt`（SOURCES 增加 `display/dashboard/dashboard_ui.cc`）

**Interfaces:**
- Consumes: Task 3 的 `WeatherSnapshot/WeatherService`、Task 4 的映射函数、Task 5 的三个字体、EMBED 的 neutral.gif（符号 `_binary_neutral_gif_start`）
- Produces（Task 7 使用）:
  - `class DashboardUI`
  - `explicit DashboardUI(lv_obj_t* parent)`
  - `void Show()` / `void Hide()` / `bool IsVisible() const`
  - `void UpdateClock()` / `void UpdateNetwork()` / `void UpdateWeather(const WeatherSnapshot&)`

- [ ] **Step 1: 写 DashboardUI 头文件**

创建 `main/display/dashboard/dashboard_ui.h`：

```cpp
#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "lvgl_display/gif/lvgl_gif.h"
#include "weather_service.h"

#include <lvgl.h>

#include <memory>

class DashboardUI {
public:
    explicit DashboardUI(lv_obj_t* parent);

    void Show();
    void Hide();
    bool IsVisible() const;

    void UpdateClock();
    void UpdateNetwork();
    void UpdateWeather(const WeatherSnapshot& snapshot);

private:
    lv_obj_t* container_ = nullptr;
    lv_obj_t* network_label_ = nullptr;
    lv_obj_t* top_clock_label_ = nullptr;
    lv_obj_t* city_label_ = nullptr;
    lv_obj_t* aqi_badge_ = nullptr;
    lv_obj_t* weather_icon_label_ = nullptr;
    lv_obj_t* aqi_line_label_ = nullptr;
    lv_obj_t* weather_text_badge_ = nullptr;
    lv_obj_t* hour_label_ = nullptr;
    lv_obj_t* colon_label_ = nullptr;
    lv_obj_t* minute_label_ = nullptr;
    lv_obj_t* second_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* weekday_label_ = nullptr;
    lv_obj_t* temp_icon_ = nullptr;
    lv_obj_t* temp_bar_ = nullptr;
    lv_obj_t* temp_value_ = nullptr;
    lv_obj_t* humid_icon_ = nullptr;
    lv_obj_t* humid_bar_ = nullptr;
    lv_obj_t* humid_value_ = nullptr;
    lv_obj_t* gif_image_ = nullptr;
    std::unique_ptr<LvglGif> gif_;
};

#endif  // DASHBOARD_UI_H
```

- [ ] **Step 2: 写界面实现**

创建 `main/display/dashboard/dashboard_ui.cc`：

```cpp
#include "dashboard_ui.h"

#include "board.h"
#include "dashboard_mappings.h"
#include "lvgl_display/lvgl_theme.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace {
LV_FONT_DECLARE(lv_font_digits_72);
LV_FONT_DECLARE(font_weather_symbols_26_4);
LV_FONT_DECLARE(font_weather_symbols_36_4);
LV_FONT_DECLARE(font_noto_sans_basic_30_4);

extern const uint8_t neutral_gif_start[] asm("_binary_neutral_gif_start");

constexpr uint32_t kFadeMs = 300;

lv_obj_t* MakeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color_hex,
                    lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color_hex), 0);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    return label;
}
}  // namespace

DashboardUI::DashboardUI(lv_obj_t* parent) {
    auto* theme = static_cast<LvglTheme*>(
        Board::GetInstance().GetDisplay()->GetTheme());
    const lv_font_t* text_font = theme->text_font()->font();
    const lv_font_t* icon_font = theme->icon_font()->font();

    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_size(container_, 240, 320);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(container_, LV_OPA_TRANSP, 0);

    // (1) Top status bar
    network_label_ = MakeLabel(container_, icon_font, 0x424242, 8, 6, 40, 24);
    top_clock_label_ = MakeLabel(container_, text_font, 0x424242, 180, 8, 52, 22);
    lv_obj_set_style_text_align(top_clock_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // (2) Weather area
    city_label_ = MakeLabel(container_, text_font, 0xF57C00, 10, 40, 88, 26);
    aqi_badge_ = MakeLabel(container_, text_font, 0xFFFFFF, 104, 40, 64, 28);
    lv_obj_set_style_bg_color(aqi_badge_, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_bg_opa(aqi_badge_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(aqi_badge_, 8, 0);
    lv_obj_set_style_pad_all(aqi_badge_, 0, 0);
    lv_obj_set_style_text_align(aqi_badge_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(aqi_badge_, "--");

    weather_icon_label_ =
        MakeLabel(container_, &font_weather_symbols_36_4, 0x607D8B, 172, 34, 60, 40);
    lv_obj_set_style_text_align(weather_icon_label_, LV_TEXT_ALIGN_CENTER, 0);

    aqi_line_label_ = MakeLabel(container_, text_font, 0x424242, 10, 82, 150, 26);
    weather_text_badge_ =
        MakeLabel(container_, text_font, 0xFFFFFF, 168, 84, 62, 24);
    lv_obj_set_style_bg_color(weather_text_badge_, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_bg_opa(weather_text_badge_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(weather_text_badge_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(weather_text_badge_, "--");

    // (3) Clock area
    hour_label_ = MakeLabel(container_, &lv_font_digits_72, 0x212121, 8, 116, 84, 78);
    colon_label_ = MakeLabel(container_, &lv_font_digits_72, 0x212121, 86, 116, 22, 78);
    minute_label_ =
        MakeLabel(container_, &lv_font_digits_72, 0xFB8C00, 106, 116, 84, 78);
    second_label_ =
        MakeLabel(container_, &font_noto_sans_basic_30_4, 0xE53935, 190, 160, 44, 34);

    // Date and weekday
    date_label_ = MakeLabel(container_, text_font, 0x616161, 10, 202, 100, 26);
    weekday_label_ = MakeLabel(container_, text_font, 0x616161, 150, 202, 80, 26);
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // (4) Environment area: thermometer row
    temp_icon_ = MakeLabel(container_, &font_weather_symbols_26_4, 0xE53935, 8, 234, 30, 28);
    char icon_utf8[5];
    CodepointToUtf8(0x1F321, icon_utf8);
    lv_label_set_text(temp_icon_, icon_utf8);

    temp_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(temp_bar_, 40, 244);
    lv_obj_set_size(temp_bar_, 84, 10);
    lv_bar_set_range(temp_bar_, 0, 100);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(temp_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0x1E88E5), LV_PART_INDICATOR);
    lv_bar_set_value(temp_bar_, 0, LV_ANIM_OFF);

    temp_value_ = MakeLabel(container_, text_font, 0x424242, 126, 234, 46, 26);

    // Humidity row
    humid_icon_ =
        MakeLabel(container_, &font_weather_symbols_26_4, 0x1E88E5, 8, 268, 30, 28);
    CodepointToUtf8(0x1F4A7, icon_utf8);
    lv_label_set_text(humid_icon_, icon_utf8);

    humid_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(humid_bar_, 40, 278);
    lv_obj_set_size(humid_bar_, 84, 10);
    lv_bar_set_range(humid_bar_, 0, 100);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(humid_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0x43A047), LV_PART_INDICATOR);
    lv_bar_set_value(humid_bar_, 0, LV_ANIM_OFF);

    humid_value_ = MakeLabel(container_, text_font, 0x424242, 126, 268, 46, 26);

    // Idle robot GIF
    static lv_img_dsc_t gif_raw;
    memset(&gif_raw, 0, sizeof(gif_raw));
    gif_raw.data = const_cast<uint8_t*>(neutral_gif_start);
    gif_ = std::make_unique<LvglGif>(&gif_raw);
    if (gif_->IsLoaded()) {
        gif_image_ = lv_image_create(container_);
        lv_obj_set_pos(gif_image_, 174, 252);
        gif_->SetFrameCallback([this]() {
            lv_image_set_src(gif_image_, gif_->image_dsc());
        });
        lv_image_set_src(gif_image_, gif_->image_dsc());
        gif_->Start();
    }

    // Self-contained timers; callbacks no-op while the dashboard is hidden.
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DashboardUI*>(lv_timer_get_user_data(timer));
            if (self->IsVisible()) {
                self->UpdateClock();
            }
        },
        1000, this);
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DashboardUI*>(lv_timer_get_user_data(timer));
            if (self->IsVisible()) {
                self->UpdateNetwork();
            }
        },
        10000, this);
}

bool DashboardUI::IsVisible() const {
    return !lv_obj_has_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void DashboardUI::Show() {
    // Cancel any in-flight hide animation first (its deleted_cb would re-hide us);
    // lv_anim_delete invokes deleted_cb, so clear HIDDEN afterwards.
    lv_anim_delete(container_, nullptr);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* parent = lv_obj_get_parent(container_);
    lv_obj_move_to_index(container_,
                         static_cast<int32_t>(lv_obj_get_child_count(parent)) - 1);
    lv_obj_fade_in(container_, kFadeMs, 0);
    UpdateClock();
    UpdateNetwork();
    UpdateWeather(WeatherService::GetInstance().GetSnapshot());
    gif_->Resume();
}

void DashboardUI::Hide() {
    // LVGL 9's lv_obj_fade_out() returns void; build the fade manually so a
    // deleted_cb can hide the container when the animation finishes/is replaced.
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, container_);
    lv_anim_set_values(&anim, lv_obj_get_style_opa(container_, 0), LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, kFadeMs);
    lv_anim_set_exec_cb(&anim, [](void* var, int32_t value) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), value, 0);
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t* anim) {
        lv_obj_t* obj = static_cast<lv_obj_t*>(anim->var);
        if (lv_obj_get_style_opa(obj, 0) == LV_OPA_TRANSP) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    });
    lv_anim_start(&anim);
    gif_->Pause();
}

void DashboardUI::UpdateClock() {
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    if (tm_now.tm_year < 2025 - 1900) {
        lv_label_set_text(top_clock_label_, "--:--");
        lv_label_set_text(hour_label_, "--");
        lv_label_set_text(colon_label_, ":");
        lv_label_set_text(minute_label_, "--");
        lv_label_set_text(second_label_, "");
        lv_label_set_text(date_label_, "--");
        lv_label_set_text(weekday_label_, "");
        return;
    }

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M", &tm_now);
    lv_label_set_text(top_clock_label_, buffer);
    strftime(buffer, sizeof(buffer), "%H", &tm_now);
    lv_label_set_text(hour_label_, buffer);
    lv_label_set_text(colon_label_, ":");
    strftime(buffer, sizeof(buffer), "%M", &tm_now);
    lv_label_set_text(minute_label_, buffer);
    strftime(buffer, sizeof(buffer), "%S", &tm_now);
    lv_label_set_text(second_label_, buffer);
    strftime(buffer, sizeof(buffer), "%m-%d", &tm_now);
    lv_label_set_text(date_label_, buffer);
    lv_label_set_text(weekday_label_, WeekdayZh(tm_now.tm_wday));
}

void DashboardUI::UpdateNetwork() {
    const char* icon = Board::GetInstance().GetNetworkStateIcon();
    if (icon != nullptr) {
        lv_label_set_text(network_label_, icon);
    }
}

void DashboardUI::UpdateWeather(const WeatherSnapshot& snapshot) {
    lv_label_set_text(city_label_,
                      (!snapshot.city.empty()) ? snapshot.city.c_str() : "--");

    AqiLevelInfo level = AqiToLevel(snapshot.aqi);
    lv_label_set_text(aqi_badge_, snapshot.aqi >= 0 ? level.category : "--");
    lv_obj_set_style_bg_color(aqi_badge_, lv_color_hex(level.color_hex), 0);
    lv_obj_set_style_text_color(aqi_badge_, lv_color_hex(level.text_color_hex), 0);

    char icon_utf8[5];
    uint32_t codepoint = WeatherIconToCodepoint(snapshot.icon_code);
    CodepointToUtf8(codepoint, icon_utf8);
    lv_label_set_text(weather_icon_label_, icon_utf8);
    lv_obj_set_style_text_color(weather_icon_label_,
                                lv_color_hex(WeatherIconColor(codepoint)), 0);

    if (!snapshot.valid) {
        const char* status_text = snapshot.key_invalid
                                      ? "密钥无效"
                                      : (snapshot.no_key ? "天气未配置" : "天气获取中...");
        lv_label_set_text(aqi_line_label_, status_text);
        lv_label_set_text(weather_text_badge_, "--");
        lv_bar_set_value(temp_bar_, 0, LV_ANIM_OFF);
        lv_bar_set_value(humid_bar_, 0, LV_ANIM_OFF);
        lv_label_set_text(temp_value_, "--°C");
        lv_label_set_text(humid_value_, "--%");
        return;
    }

    char line[32];
    if (snapshot.aqi >= 0) {
        snprintf(line, sizeof(line), "空气指数 %d", snapshot.aqi);
        lv_label_set_text(aqi_line_label_, line);
    } else {
        lv_label_set_text(aqi_line_label_, "空气指数 --");
    }
    lv_label_set_text(weather_text_badge_, snapshot.weather_text.c_str());

    lv_bar_set_value(temp_bar_, TempToPercent(snapshot.temperature), LV_ANIM_OFF);
    lv_bar_set_value(humid_bar_, snapshot.humidity, LV_ANIM_OFF);
    snprintf(line, sizeof(line), "%d°C", snapshot.temperature);
    lv_label_set_text(temp_value_, line);
    snprintf(line, sizeof(line), "%d%%", snapshot.humidity);
    lv_label_set_text(humid_value_, line);
}
```

- [ ] **Step 3: 加入 CMake SOURCES**

在 `main/CMakeLists.txt` 的 `if(CONFIG_WEATHER_DASHBOARD)` SOURCES 中追加：

```cmake
        "display/dashboard/dashboard_ui.cc"
```

- [ ] **Step 4: 固件编译验证**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。

- [ ] **Step 5: 提交**

```bash
git add main/display/dashboard/dashboard_ui.h \
    main/display/dashboard/dashboard_ui.cc main/CMakeLists.txt
git commit -m "feat(dashboard): add LVGL standby dashboard UI with clock, weather and GIF"
```

---

### Task 7: LcdDisplay 与 Application 集成

**Files:**
- Modify: `main/display/display.h`
- Modify: `main/display/lcd_display.h`
- Modify: `main/display/lcd_display.cc`
- Modify: `main/application.cc`

**Interfaces:**
- Consumes: Task 6 的 `DashboardUI`、Task 3 的 `WeatherService`
- Produces: `Display::ShowDashboard()` / `Display::HideDashboard()`；Application idle 时自动切换

- [ ] **Step 1: Display 基类增加默认空实现**

在 `main/display/display.h` 的 `Display` 类 public 区域（`SetHideSubtitle` 附近）加：

```cpp
    virtual void ShowDashboard() {}
    virtual void HideDashboard() {}
```

- [ ] **Step 2: LcdDisplay 持有 DashboardUI**

在 `main/display/lcd_display.h` 顶部 includes 之后加前向声明（文件内已有 `#include <memory>`）：

```cpp
#if CONFIG_WEATHER_DASHBOARD
class DashboardUI;
#endif
```

在 `protected:` 成员区（`bool hide_subtitle_` 附近）加：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    std::unique_ptr<DashboardUI> dashboard_;
    void SetupDashboard();
#endif
```

注意：本文件有两个条件编译的 `SetupUI()` 实现（`#if CONFIG_USE_WECHAT_MESSAGE_STYLE` 微信版约 :388-536，`#else` 经典版约 :854-1118）。dashboard 初始化必须在**两个版本**中都执行，因此用 helper `SetupDashboard()` 复用。

在 `public:` 区（`SetHideSubtitle` 声明附近）加：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    virtual void ShowDashboard() override;
    virtual void HideDashboard() override;
#endif
```

- [ ] **Step 3: lcd_display.cc 创建 UI 并实现 Show/Hide**

在 `main/display/lcd_display.cc` 顶部 include 区加：

```cpp
#if CONFIG_WEATHER_DASHBOARD
#include "application.h"
#include "weather_service.h"
#include "dashboard_ui.h"
#endif
```

在**微信版** `SetupUI()` 的最后一条语句（`lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);`，约 :535）之后、函数结束 `}` 之前插入：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    SetupDashboard();
#endif
```

在**经典版** `SetupUI()` 末尾（约 :1115-1118，内层 `#if ... #endif` 之后、函数结束 `}` 之前）插入同样三行：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    SetupDashboard();
#endif
```

在两个版本之外的公共区域——`#endif`（约 :1130）之后、`void LcdDisplay::SetEmotion(const char* emotion) {`（约 :1132）之前——插入 helper 与 Show/Hide 方法实现：

```cpp
#if CONFIG_WEATHER_DASHBOARD
void LcdDisplay::SetupDashboard() {
    dashboard_ = std::make_unique<DashboardUI>(lv_screen_active());
    WeatherService::GetInstance().SetUpdateCallback([this]() {
        Application::GetInstance().Schedule([this]() {
            if (dashboard_) {
                dashboard_->UpdateWeather(WeatherService::GetInstance().GetSnapshot());
            }
        });
    });
}

void LcdDisplay::ShowDashboard() {
    if (dashboard_) {
        dashboard_->Show();
        dashboard_->UpdateWeather(WeatherService::GetInstance().GetSnapshot());
    }
}

void LcdDisplay::HideDashboard() {
    if (dashboard_) {
        dashboard_->Hide();
    }
}
#endif
```

- [ ] **Step 4: Application 状态切换驱动仪表盘**

在 `main/application.cc` 顶部 include 区加（必须条件编译——该配置在其他板子默认 n，weather 目录不参与编译）：

```cpp
#if CONFIG_WEATHER_DASHBOARD
#include "weather_service.h"
#endif
```

在 `HandleStateChangedEvent()` 中 `led->OnStateChanged();` 之后、`switch (new_state)` 之前插入：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    if (new_state != kDeviceStateIdle && new_state != kDeviceStateUnknown) {
        display->HideDashboard();
    }
#endif
```

修改 idle 分支中 `if (last_error_message_.empty()) { ... }` 块，在 `display->SetEmotion("neutral");` 之后加一行：

```cpp
#if CONFIG_WEATHER_DASHBOARD
                display->ShowDashboard();
#endif
```

修改后的该块为：

```cpp
            if (last_error_message_.empty()) {
                display->SetStatus(Lang::Strings::STANDBY);
                display->ClearChatMessages();  // Clear messages first
                display->SetEmotion(
                    "neutral");  // Then set emotion (wechat mode checks child count)
#if CONFIG_WEATHER_DASHBOARD
                display->ShowDashboard();
#endif
            }
```

- [ ] **Step 5: Application 网络事件驱动 WeatherService**

在 `HandleNetworkConnectedEvent()` 末尾（`display->UpdateStatusBar(true);` 之后）加：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    WeatherService::GetInstance().OnNetworkConnected();
    WeatherService::GetInstance().Start();
#endif
```

在 `HandleNetworkDisconnectedEvent()` 末尾（`display->UpdateStatusBar(true);` 之后）加：

```cpp
#if CONFIG_WEATHER_DASHBOARD
    WeatherService::GetInstance().OnNetworkDisconnected();
#endif
```

- [ ] **Step 6: 固件编译验证**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。

- [ ] **Step 7: 提交**

```bash
git add main/display/display.h main/display/lcd_display.h main/display/lcd_display.cc \
    main/application.cc
git commit -m "feat: wire standby dashboard into LcdDisplay and Application idle state"
```

---

### Task 8: 板子注册天气密钥 MCP 工具

**Files:**
- Modify: `main/boards/lcdwiki-es3c28p/lcdwiki-es3c28p.cc`

**Interfaces:**
- Consumes: `WeatherKeyStore::SetKey`、`WeatherService::OnKeyUpdated`
- Produces: MCP 工具 `self.system.set_weather_api_key`（参数 `key`，最长 64）

- [ ] **Step 1: 添加 include**

在 `main/boards/lcdwiki-es3c28p/lcdwiki-es3c28p.cc` 的 include 区（`#include "mcp_server.h"` 附近）加：

```cpp
#include "weather_key_store.h"
#include "weather_service.h"
```

- [ ] **Step 2: 在 InitializeTools 中注册工具**

在 `InitializeTools()` 中已有的 `mcp_server.AddTool("self.system.reconfigure_wifi", ...)` 之后追加：

```cpp
        mcp_server.AddTool(
            "self.system.set_weather_api_key",
            "Set the QWeather (和风天气) API key used by the standby dashboard. "
            "The key is stored locally on this device in NVS. "
            "You must ask the user to confirm before calling this tool.",
            PropertyList({Property("key", kPropertyTypeString).SetMaxLength(64)}),
            [](const PropertyList& properties) -> ToolResult {
                auto key = properties["key"].value<std::string>();
                WeatherKeyStore key_store;
                key_store.SetKey(key);
                WeatherService::GetInstance().OnKeyUpdated();
                return std::string("天气密钥已保存");
            });
```

- [ ] **Step 3: 全量固件编译验证**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete`，0 error、0 warning。

- [ ] **Step 4: 提交**

```bash
git add main/boards/lcdwiki-es3c28p/lcdwiki-es3c28p.cc
git commit -m "feat(board): add MCP tool to configure QWeather API key by voice"
```

---

### Task 9: 实机验证

**Files:** 无代码改动；使用 Task 8 产出的固件。

- [ ] **Step 1: 烧录固件并监视日志**

```bash
source /Users/dairibao/esp/v6.0.2/esp-idf/export.sh
idf.py -p /dev/cu.usbmodem* flash monitor
```

（串口名按 `ls /dev/cu.*` 实际值替换；退出 monitor 用 Ctrl-]。）

- [ ] **Step 2: 未配置 Key 的待机界面**

设备联网激活并回到 idle 后，确认：

- 仪表盘淡入，顶部 WiFi 图标与小字时间正常
- 天气区显示"天气未配置"，温湿度显示 `--°C` / `--%`
- 右下角机器人 GIF 在动
- 日志中无崩溃或反复重启

- [ ] **Step 3: 语音设置 Key**

对设备说："使用设置天气密钥工具，密钥是 <你的和风 Key>"（或触发对话后让 AI 调用 `self.system.set_weather_api_key`）。确认：

- 设备回复"天气密钥已保存"
- 5 分钟内（Key 更新立即唤醒任务）日志依次出现 `Located in <城市>`、`Weather updated: ...`
- 界面显示真实城市、AQI 徽章与等级、天气图标、空气指数、天气文字、温湿度进度条与数值
- 和风面板后台可见设备的调用计数增加

- [ ] **Step 4: 聊天与待机切换**

- 按 BOOT 键或说唤醒词：仪表盘淡出，气泡聊天界面出现，对话功能正常
- 对话结束回到 idle：仪表盘淡入，数据仍在
- 时钟秒数每秒跳动；跨分钟/跨小时时观察变化正确

- [ ] **Step 5: 断网与重连**

- 关闭路由器（或断电 AP）：WiFi 图标变成 off，日志出现 `Offline, waiting for network`，无异常重启
- 恢复网络：图标恢复，日志出现重新定位/刷新，界面数据更新

- [ ] **Step 6: 最终状态确认**

- `idf.py build` 输出 0 error、0 warning
- `git status` 干净；分支提交历史包含 Task 1-8 的 8 个提交
- 如全部通过，告知用户功能完成；可按需 `git push origin feat/lcdwiki-es3c28p`
