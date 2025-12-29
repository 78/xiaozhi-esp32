#include "weather_service.h"
#include "weather_config.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstring>

#define TAG "WeatherService"

WeatherService::WeatherService() : last_update_time_(0)
{
    api_key_ = OPEN_WEATHERMAP_API_KEY;
    city_ = WEATHER_CITY_DEFAULT;
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

bool WeatherService::FetchWeatherData()
{
    if (is_fetching_)
        return false;
    is_fetching_ = true;

    // 1. Fetch Current Weather (and get Lat/Lon)
    if (!FetchCurrentWeather(city_, api_key_))
    {
        is_fetching_ = false;
        return false;
    }

    // 2. Fetch Air Pollution (PM2.5) using Lat/Lon
    FetchAirPollution(lat_, lon_, api_key_);

    last_update_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        weather_info_.valid = true;
    }

    is_fetching_ = false;
    return true;
}

bool WeatherService::FetchCurrentWeather(const std::string &city, const std::string &api_key)
{
    std::string url = std::string(WEATHER_API_ENDPOINT) + "?q=" + UrlEncode(city) +
                      "&appid=" + api_key + "&units=metric&lang=vi";

    std::string response = HttpGet(url);
    if (response.empty())
        return false;

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
        return false;

    // Temporary struct to hold data before locking
    WeatherInfo temp_info;

    // Parse Coord
    cJSON *coord = cJSON_GetObjectItem(json, "coord");
    if (coord)
    {
        lat_ = cJSON_GetObjectItem(coord, "lat")->valuedouble;
        lon_ = cJSON_GetObjectItem(coord, "lon")->valuedouble;
    }

    // Parse Main
    cJSON *main = cJSON_GetObjectItem(json, "main");
    if (main)
    {
        temp_info.temp = cJSON_GetObjectItem(main, "temp")->valuedouble;
        temp_info.humidity = cJSON_GetObjectItem(main, "humidity")->valueint;
        temp_info.pressure = cJSON_GetObjectItem(main, "pressure")->valueint;
        temp_info.feels_like = cJSON_GetObjectItem(main, "feels_like")->valuedouble;
    }

    // Parse Weather
    cJSON *weather_array = cJSON_GetObjectItem(json, "weather");
    if (weather_array && cJSON_GetArraySize(weather_array) > 0)
    {
        cJSON *weather_item = cJSON_GetArrayItem(weather_array, 0);
        temp_info.description = cJSON_GetObjectItem(weather_item, "description")->valuestring;
        temp_info.icon_code = cJSON_GetObjectItem(weather_item, "icon")->valuestring;
    }

    // Parse Wind
    cJSON *wind = cJSON_GetObjectItem(json, "wind");
    if (wind)
    {
        temp_info.wind_speed = cJSON_GetObjectItem(wind, "speed")->valuedouble;
    }

    // Parse Name
    cJSON *name = cJSON_GetObjectItem(json, "name");
    if (name)
    {
        temp_info.city = name->valuestring;
    }

    cJSON_Delete(json);

    // Update shared state
    {
        std::lock_guard<std::mutex> lock(mutex_);
        weather_info_.temp = temp_info.temp;
        weather_info_.humidity = temp_info.humidity;
        weather_info_.pressure = temp_info.pressure;
        weather_info_.feels_like = temp_info.feels_like;
        weather_info_.description = temp_info.description;
        weather_info_.icon_code = temp_info.icon_code;
        weather_info_.wind_speed = temp_info.wind_speed;
        weather_info_.city = temp_info.city;
    }

    return true;
}

bool WeatherService::FetchAirPollution(float lat, float lon, const std::string &api_key)
{
    char url[256];
    snprintf(url, sizeof(url), "%s?lat=%f&lon=%f&appid=%s", AIR_POLLUTION_API_ENDPOINT, lat, lon, api_key.c_str());

    std::string response = HttpGet(url);
    if (response.empty())
        return false;

    cJSON *json = cJSON_Parse(response.c_str());
    if (!json)
        return false;

    double pm2_5 = 0.0;

    // Parse List
    cJSON *list = cJSON_GetObjectItem(json, "list");
    if (list && cJSON_GetArraySize(list) > 0)
    {
        cJSON *item = cJSON_GetArrayItem(list, 0);
        cJSON *components = cJSON_GetObjectItem(item, "components");
        if (components)
        {
            pm2_5 = cJSON_GetObjectItem(components, "pm2_5")->valuedouble;
        }
    }

    cJSON_Delete(json);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        weather_info_.pm2_5 = pm2_5;
    }

    return true;
}
