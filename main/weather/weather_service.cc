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
