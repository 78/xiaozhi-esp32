#include "dashboard_fetcher.h"

#include "application.h"
#include "board.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ctime>
#include <memory>

#define TAG "DashboardFetcher"

namespace {

constexpr int kTaskStackBytes = 10 * 1024;
constexpr UBaseType_t kTaskPriority = 2;
constexpr int64_t kPollSeconds = 30;
// While a source has never loaded, check often and retry soon: the screen shows
// "loading..." and the failure is usually a slow start (DNS, WiFi, TLS).
constexpr int64_t kFirstLoadPollSeconds = 3;
constexpr int64_t kFirstLoadRetrySeconds = 20;
constexpr int64_t kWeatherRefreshSeconds = 30 * 60;
constexpr int64_t kReposRefreshSeconds = 60 * 60;
constexpr int64_t kRetrySeconds = 5 * 60;
constexpr int kHttpTimeoutMs = 10000;
constexpr size_t kMaxBodyBytes = 64 * 1024;
// A TLS session needs internal RAM; below this free amount a fetch is skipped.
constexpr size_t kMinFreeInternalBytes = 60 * 1024;

const char kUserAgent[] = "xiaozhi-jc8048w550";
const char kGeoUrl[] = "https://ipwho.is/";
// Open-Meteo: current conditions plus 8 daily entries (today and 7 more days).
const char kWeatherUrlFormat[] =
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,relative_humidity_2m,weather_code"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=8";

int64_t NowSeconds() { return esp_timer_get_time() / 1000000; }

// The system clock is set from the OTA server's time; before that it is 1970.
bool TimeSynced() {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    return local.tm_year >= 2025 - 1900;
}

std::string RepoSearchUrl() {
    time_t week_ago = time(nullptr) - 7 * 24 * 60 * 60;
    struct tm utc;
    gmtime_r(&week_ago, &utc);
    char date[16];
    strftime(date, sizeof(date), "%Y-%m-%d", &utc);
    // Repos tagged llm created in the last week, most starred first.
    return std::string("https://api.github.com/search/repositories?q=topic:llm+created:>") + date +
           "&sort=stars&order=desc&per_page=5";
}

}  // namespace

DashboardFetcher::DashboardFetcher(Listener on_update) : on_update_(std::move(on_update)) {}

void DashboardFetcher::Start() {
    if (xTaskCreate(TaskEntry, "dashboard_fetch", kTaskStackBytes, this, kTaskPriority, nullptr) !=
        pdPASS) {
        ESP_LOGE(TAG, "Failed to create the fetch task");
    }
}

void DashboardFetcher::TaskEntry(void* arg) {
    static_cast<DashboardFetcher*>(arg)->Run();
    vTaskDelete(nullptr);
}

void DashboardFetcher::Run() {
    // Let startup settle before the first request.
    vTaskDelay(pdMS_TO_TICKS(5000));
    while (true) {
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        bool allowed = dashboard::FetchAllowed(Application::GetInstance().GetDeviceState(),
                                               TimeSynced(), free_internal, kMinFreeInternalBytes);
        int64_t now = NowSeconds();
        bool changed = false;
        if (allowed && now >= weather_due_s_) {
            bool ok = RefreshWeather();
            int64_t retry = snapshot_.weather.valid ? kRetrySeconds : kFirstLoadRetrySeconds;
            weather_due_s_ = now + (ok ? kWeatherRefreshSeconds : retry);
            changed = true;
        }
        if (allowed && now >= repos_due_s_) {
            bool ok = RefreshRepos();
            int64_t retry = snapshot_.repos.valid ? kRetrySeconds : kFirstLoadRetrySeconds;
            repos_due_s_ = now + (ok ? kReposRefreshSeconds : retry);
            changed = true;
        }
        if (changed && on_update_) {
            on_update_(snapshot_);
        }
        bool loading = !snapshot_.weather.valid || !snapshot_.repos.valid;
        vTaskDelay(pdMS_TO_TICKS((loading ? kFirstLoadPollSeconds : kPollSeconds) * 1000));
    }
}

bool DashboardFetcher::HttpGet(const std::string& url, std::string& body) {
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        return false;
    }
    size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    auto http = network->CreateHttp(3);
    http->SetTimeout(kHttpTimeoutMs);
    http->SetHeader("User-Agent", kUserAgent);
    http->SetHeader("Accept", "application/json");
    bool ok = false;
    if (auto opened = http->Open("GET", url); !opened) {
        ESP_LOGW(TAG, "Open %s failed: %s", url.c_str(), opened.error().ToString().c_str());
    } else if (auto status = http->GetStatusCode(); !status || *status != 200) {
        ESP_LOGW(TAG, "GET %s -> status %d", url.c_str(), status ? *status : -1);
    } else if (http->GetBodyLength() > kMaxBodyBytes) {
        ESP_LOGW(TAG, "GET %s: body too large", url.c_str());
    } else {
        body = http->ReadAll();
        ok = !body.empty() && body.size() <= kMaxBodyBytes;
    }
    http->Close();
    http.reset();
    ESP_LOGI(TAG, "GET ok=%d internal free %u -> %u (min ever %u)", ok,
             static_cast<unsigned>(internal_before),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    return ok;
}

bool DashboardFetcher::RefreshLocation() {
    std::string body;
    dashboard::GeoLocation location;
    if (!HttpGet(kGeoUrl, body) || !dashboard::ParseGeoIp(body, location)) {
        return false;
    }
    location_ = std::move(location);
    have_location_ = true;
    return true;
}

bool DashboardFetcher::RefreshWeather() {
    auto& source = snapshot_.weather;
    // The location comes from the IP once per boot; a failed weather request
    // does not throw it away.
    bool ok = (have_location_ || RefreshLocation());
    if (ok) {
        char url[320];
        snprintf(url, sizeof(url), kWeatherUrlFormat, location_.latitude, location_.longitude);
        std::string body;
        dashboard::Weather weather;
        ok = HttpGet(url, body) && dashboard::ParseWeather(body, location_.city, weather);
        if (ok) {
            source.data = std::move(weather);
            source.valid = true;
            source.fetched_at_s = time(nullptr);
        }
    }
    source.last_failed = !ok;
    ESP_LOGI(TAG, "weather: ok=%d valid=%d city=%s temp=%d", ok, source.valid,
             source.data.city.c_str(), source.data.temp_c);
    return ok;
}

bool DashboardFetcher::RefreshRepos() {
    auto& source = snapshot_.repos;
    std::string body;
    std::vector<dashboard::Repo> repos;
    bool ok = HttpGet(RepoSearchUrl(), body) && dashboard::ParseRepos(body, repos);
    if (ok) {
        source.data = std::move(repos);
        source.valid = true;
        source.fetched_at_s = time(nullptr);
    }
    source.last_failed = !ok;
    ESP_LOGI(TAG, "repos: ok=%d valid=%d n=%u top=%s stars=%d", ok, source.valid,
             static_cast<unsigned>(source.data.size()),
             source.data.empty() ? "-" : source.data[0].full_name.c_str(),
             source.data.empty() ? 0 : source.data[0].stars);
    return ok;
}
