#include "weather_parsers.h"

#include "cjson_utils.h"

#include <cstdlib>

namespace {
std::string GetStringField(cJSON* obj, const char* key) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return field != nullptr && cJSON_IsString(field) ? std::string(field->valuestring)
                                                     : std::string();
}

int GetIntField(cJSON* obj, const char* key) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    if (field == nullptr) {
        return 0;
    }
    if (cJSON_IsNumber(field)) {
        return field->valueint;
    }
    if (cJSON_IsString(field)) {
        try {
            return std::stoi(field->valuestring);
        } catch (...) {
            return 0;
        }
    }
    return 0;
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
    cJSON* lat = cJSON_GetObjectItem(root.get(), "lat");
    cJSON* lon = cJSON_GetObjectItem(root.get(), "lon");
    if (lat == nullptr || lon == nullptr || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        return geo;
    }
    geo.ok = true;
    geo.city = GetStringField(root.get(), "city");
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
    data.ok = true;
    data.text = GetStringField(now, "text");
    data.icon = GetStringField(now, "icon");
    data.temperature = GetIntField(now, "temp");
    data.humidity = GetIntField(now, "humidity");
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
