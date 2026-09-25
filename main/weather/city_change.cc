#include "city_change.h"

#include "http_get.h"
#include "weather_city_store.h"
#include "weather_key_store.h"
#include "weather_parsers.h"
#include "weather_service.h"

#include <esp_log.h>

#define TAG "CityChange"

CityChangeResult ChangeCity(const std::string& city_name) {
    CityChangeResult result;
    if (city_name.empty()) {
        result.message = "城市名不能为空";
        return result;
    }

    std::string key = WeatherKeyStore().GetKey();
    if (key.empty()) {
        result.message = "天气服务还没有配置和风天气的 API Key，暂时无法切换城市";
        return result;
    }

    std::string url = "https://geoapi.qweather.com/v2/city/lookup?location=" +
                      UrlEncode(city_name) + "&number=1&key=" + key;
    auto response = HttpGet(url, 8000);
    CityLookupData lookup = ParseCityLookupResponse(response.body, response.status);
    result.key_invalid = lookup.key_invalid;
    if (lookup.key_invalid) {
        result.message = "和风天气的 API Key 无效，无法切换城市";
        return result;
    }
    if (!lookup.ok) {
        result.message = "没有找到名为「" + city_name + "」的城市，换个名字试试吧";
        return result;
    }

    WeatherCityStore city_store;
    CityLocation current = city_store.GetCity();
    if (current.found && current.city == lookup.geo.city) {
        result.ok = true;
        result.message = "当前城市已经是" + lookup.geo.city + "啦";
        return result;
    }

    city_store.SetCity(lookup.geo.city, lookup.geo.lat, lookup.geo.lon);
    WeatherService::GetInstance().OnCityUpdated();
    ESP_LOGI(TAG, "Manual city set to %s (%.4f, %.4f)", lookup.geo.city.c_str(),
             lookup.geo.lat, lookup.geo.lon);

    result.ok = true;
    result.message = "好的，已经把城市切换为" + lookup.geo.city + "，正在为你刷新天气";
    return result;
}
