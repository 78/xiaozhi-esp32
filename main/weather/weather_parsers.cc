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
