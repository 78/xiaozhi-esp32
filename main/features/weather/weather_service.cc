#include "weather_service.h"
#include "weather_config.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstring>

#define TAG "WeatherService"

WeatherService::WeatherService() : last_update_time_(0), location_initialized_(false)
{
    city_ = "Ho Chi Minh";
    lat_ = 10.8231;
    lon_ = 106.6297;
    weather_info_.city = city_;
}

WeatherInfo WeatherService::GetWeatherInfo()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return weather_info_;
}

bool WeatherService::NeedsUpdate() const
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (current_time - last_update_time_) >= WEATHER_UPDATE_INTERVAL_MS;
}

std::string WeatherService::CleanCityName(const std::string &city)
{
    std::string clean = city;
    // List of suffixes to remove. Includes user request and common variations.
    const char *suffixes[] = {
        " City", " Province", " Town", " District", " Municipality",
        " city", " province", " town", " district", " municipality"};

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const char *suffix : suffixes)
        {
            size_t suffix_len = strlen(suffix);
            // Check if suffix matches and ensure we don't remove the entire string (e.g. city named "Town")
            if (clean.size() > suffix_len &&
                clean.compare(clean.size() - suffix_len, suffix_len, suffix) == 0)
            {
                clean = clean.substr(0, clean.size() - suffix_len);
                changed = true;
                break; // Restart loop to handle stacked suffixes like "City District"
            }
        }
    }
    return clean;
}

std::string WeatherService::UrlEncode(const std::string &value)
{
    // Simple URL encoding
    std::string escaped;
    for (char c : value)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped += c;
        }
        else
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            escaped += buf;
        }
    }
    return escaped;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static std::string *response_buffer;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (evt->user_data)
        {
            response_buffer = (std::string *)evt->user_data;
            response_buffer->append((char *)evt->data, evt->data_len);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static std::string HttpGet(const std::string &url)
{
    std::string response_buffer;
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.event_handler = _http_event_handler;
    config.user_data = &response_buffer;
    config.timeout_ms = WEATHER_HTTP_TIMEOUT_MS;
    config.buffer_size = 4096; // Increase buffer for larger JSON
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return response_buffer;
}

// Helper to map WMO codes to OpenWeatherMap icon codes
static std::string WmoCodeToIcon(int code)
{
    // 0: Clear sky
    if (code == 0)
        return "01d";
    // 1, 2, 3: Mainly clear, partly cloudy, and overcast
    if (code == 1)
        return "01d";
    if (code == 2)
        return "02d";
    if (code == 3)
        return "03d"; // or 04d
    // 45, 48: Fog and depositing rime fog
    if (code == 45 || code == 48)
        return "50d";
    // 51, 53, 55: Drizzle: Light, moderate, and dense intensity
    if (code >= 51 && code <= 55)
        return "09d";
    // 56, 57: Freezing Drizzle: Light and dense intensity
    if (code == 56 || code == 57)
        return "09d";
    // 61, 63, 65: Rain: Slight, moderate and heavy intensity
    if (code >= 61 && code <= 65)
        return "10d";
    // 66, 67: Freezing Rain: Light and heavy intensity
    if (code == 66 || code == 67)
        return "13d";
    // 71, 73, 75: Snow fall: Slight, moderate, and heavy intensity
    if (code >= 71 && code <= 75)
        return "13d";
    // 77: Snow grains
    if (code == 77)
        return "13d";
    // 80, 81, 82: Rain showers: Slight, moderate, and violent
    if (code >= 80 && code <= 82)
        return "09d";
    // 85, 86: Snow showers slight and heavy
    if (code == 85 || code == 86)
        return "13d";
    // 95: Thunderstorm: Slight or moderate
    if (code == 95)
        return "11d";
    // 96, 99: Thunderstorm with slight and heavy hail
    if (code == 96 || code == 99)
        return "11d";

    return "01d"; // Default
}

static std::string WmoCodeToDescription(int code)
{
    switch (code)
    {
    case 0:
        return "Clear sky";
    case 1:
        return "Mainly clear";
    case 2:
        return "Partly cloudy";
    case 3:
        return "Overcast";
    case 45:
        return "Fog";
    case 48:
        return "Depositing rime fog";
    case 51:
        return "Light drizzle";
    case 53:
        return "Moderate drizzle";
    case 55:
        return "Dense drizzle";
    case 56:
        return "Light freezing drizzle";
    case 57:
        return "Dense freezing drizzle";
    case 61:
        return "Slight rain";
    case 63:
        return "Moderate rain";
    case 65:
        return "Heavy rain";
    case 66:
        return "Light freezing rain";
    case 67:
        return "Heavy freezing rain";
    case 71:
        return "Slight snow fall";
    case 73:
        return "Moderate snow fall";
    case 75:
        return "Heavy snow fall";
    case 77:
        return "Snow grains";
    case 80:
        return "Slight rain showers";
    case 81:
        return "Moderate rain showers";
    case 82:
        return "Violent rain showers";
    case 85:
        return "Slight snow showers";
    case 86:
        return "Heavy snow showers";
    case 95:
        return "Thunderstorm";
    case 96:
        return "Thunderstorm with slight hail";
    case 99:
        return "Thunderstorm with heavy hail";
    default:
        return "Unknown";
    }
}

bool WeatherService::FetchWeatherData()
{
    if (is_fetching_)
        return false;
    is_fetching_ = true;

    // 0. Auto-detect location if not initialized
    if (!location_initialized_)
    {
        if (FetchLocationFromIP())
        {
            ESP_LOGI(TAG, "Location auto-detected successfully");
        }
        else
        {
            ESP_LOGW(TAG, "Location auto-detection failed, using default: %s", city_.c_str());
        }
    }

    // 1. Geocoding to get Lat/Lon from City Name
    // if (!FetchGeocoding(city_))
    // {
    //     ESP_LOGE(TAG, "Geocoding failed for city: %s", city_.c_str());
    //     is_fetching_ = false;
    //     return false;
    // }

    // 2. Fetch Weather Data
    if (!FetchOpenMeteoWeather(lat_, lon_))
    {
        ESP_LOGE(TAG, "Weather fetch failed");
        is_fetching_ = false;
        return false;
    }

    // 3. Fetch Air Quality
    FetchOpenMeteoAirQuality(lat_, lon_);

    last_update_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        weather_info_.valid = true;
    }

    is_fetching_ = false;
    return true;
}

bool WeatherService::FetchLocationFromIP()
{
    std::string url = IP_LOCATION_API_ENDPOINT;
    std::string response = HttpGet(url);
    if (response.empty())
    {
        ESP_LOGE(TAG, "Failed to fetch location from IP");
        return false;
    }

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse IP location JSON");
        return false;
    }

    bool success = false;
    cJSON *success_item = cJSON_GetObjectItem(json, "success");
    if (success_item && cJSON_IsTrue(success_item))
    {
        cJSON *city_item = cJSON_GetObjectItem(json, "city");
        cJSON *lat_item = cJSON_GetObjectItem(json, "latitude");
        cJSON *lon_item = cJSON_GetObjectItem(json, "longitude");

        if (city_item && lat_item && lon_item)
        {
            lat_ = lat_item->valuedouble;
            lon_ = lon_item->valuedouble;
            city_ = CleanCityName(city_item->valuestring);

            {
                std::lock_guard<std::mutex> lock(mutex_);
                weather_info_.city = city_;
            }

            ESP_LOGI(TAG, "Location detected: %s (%.4f, %.4f)", city_.c_str(), lat_, lon_);
            success = true;
            location_initialized_ = true;
        }
    }
    else
    {
        ESP_LOGE(TAG, "IP location API returned error");
    }

    cJSON_Delete(json);
    return success;
}

bool WeatherService::FetchGeocoding(const std::string &city)
{
    // https://geocoding-api.open-meteo.com/v1/search?name=Berlin&count=1&language=en&format=json
    std::string url = "https://geocoding-api.open-meteo.com/v1/search?name=" + UrlEncode(city) + "&count=1&language=en&format=json";

    std::string response = HttpGet(url);
    if (response.empty())
        return false;

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
        return false;

    bool success = false;
    cJSON *results = cJSON_GetObjectItem(json, "results");
    if (results && cJSON_GetArraySize(results) > 0)
    {
        cJSON *item = cJSON_GetArrayItem(results, 0);
        cJSON *lat_item = cJSON_GetObjectItem(item, "latitude");
        cJSON *lon_item = cJSON_GetObjectItem(item, "longitude");
        cJSON *name_item = cJSON_GetObjectItem(item, "name");

        if (lat_item && lon_item)
        {
            lat_ = lat_item->valuedouble;
            lon_ = lon_item->valuedouble;

            // Update city name with the resolved one (optional, but good for display)
            if (name_item && name_item->valuestring)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                weather_info_.city = name_item->valuestring;
            }
            success = true;
        }
    }

    cJSON_Delete(json);
    return success;
}

bool WeatherService::FetchOpenMeteoWeather(float lat, float lon)
{
    // https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,pressure_msl,wind_speed_10m
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,pressure_msl,wind_speed_10m",
             lat, lon);

    std::string response = HttpGet(url);
    if (response.empty())
        return false;

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
        return false;

    bool success = false;
    cJSON *current = cJSON_GetObjectItem(json, "current");
    if (current)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        cJSON *temp = cJSON_GetObjectItem(current, "temperature_2m");
        if (temp)
            weather_info_.temp = temp->valuedouble;

        cJSON *hum = cJSON_GetObjectItem(current, "relative_humidity_2m");
        if (hum)
            weather_info_.humidity = hum->valueint;

        cJSON *press = cJSON_GetObjectItem(current, "pressure_msl");
        if (press)
            weather_info_.pressure = (int)press->valuedouble;

        cJSON *feels = cJSON_GetObjectItem(current, "apparent_temperature");
        if (feels)
            weather_info_.feels_like = feels->valuedouble;

        cJSON *wind = cJSON_GetObjectItem(current, "wind_speed_10m");
        if (wind)
            weather_info_.wind_speed = wind->valuedouble;

        cJSON *code = cJSON_GetObjectItem(current, "weather_code");
        if (code)
        {
            int wmo_code = code->valueint;
            weather_info_.icon_code = WmoCodeToIcon(wmo_code);
            weather_info_.description = WmoCodeToDescription(wmo_code);
        }

        success = true;
    }

    cJSON_Delete(json);
    return success;
}

bool WeatherService::FetchOpenMeteoAirQuality(float lat, float lon)
{
    // https://air-quality-api.open-meteo.com/v1/air-quality?latitude=52.52&longitude=13.41&current=pm2_5,uv_index
    char url[256];
    snprintf(url, sizeof(url),
             "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f&current=pm2_5,uv_index",
             lat, lon);

    std::string response = HttpGet(url);
    if (response.empty())
        return false;

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
        return false;

    bool success = false;
    cJSON *current = cJSON_GetObjectItem(json, "current");
    if (current)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        cJSON *pm25 = cJSON_GetObjectItem(current, "pm2_5");
        if (pm25)
        {
            weather_info_.pm2_5 = pm25->valuedouble;
        }

        cJSON *uv = cJSON_GetObjectItem(current, "uv_index");
        if (uv)
        {
            weather_info_.uv_index = uv->valuedouble;
        }

        success = true;
    }

    cJSON_Delete(json);
    return success;
}
