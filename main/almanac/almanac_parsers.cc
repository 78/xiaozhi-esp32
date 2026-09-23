#include "almanac_parsers.h"

#include "cjson_utils.h"

#include <cctype>

namespace {

std::string GetStringField(cJSON* obj, const char* key) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return field != nullptr && cJSON_IsString(field) ? std::string(field->valuestring)
                                                     : std::string();
}

// Juhe joins activities with spaces, e.g. "装修 沐浴 祭祀 馀事勿取 铺路".
std::vector<std::string> SplitActivities(const std::string& text) {
    std::vector<std::string> items;
    std::string current;
    auto flush = [&]() {
        if (!current.empty()) {
            items.push_back(current);
            current.clear();
        }
    };
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();
    return items;
}

}  // namespace

AlmanacData ParseAlmanacResponse(const std::string& body, int http_status) {
    AlmanacData data;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        data.credentials_invalid = (http_status == 401 || http_status == 403);
        return data;
    }

    // Success is error_code == 0. Codes per juhe.cn/docs/api/id/65:
    // 10001 wrong key, 10002 no permission, 10003 expired, 10004 bad openid,
    // 10009 banned key -> user must fix the key; 10012/10013 -> daily quota.
    cJSON* error_code = cJSON_GetObjectItem(root.get(), "error_code");
    if (error_code == nullptr || !cJSON_IsNumber(error_code)) {
        return data;
    }
    if (error_code->valueint != 0) {
        int code = error_code->valueint;
        if (http_status == 401 || http_status == 403 ||
            code == 10001 || code == 10002 || code == 10003 ||
            code == 10004 || code == 10009) {
            data.credentials_invalid = true;
        } else if (code == 10012 || code == 10013) {
            data.quota_exceeded = true;
        }
        return data;
    }

    cJSON* result = cJSON_GetObjectItem(root.get(), "result");
    if (result == nullptr || !cJSON_IsObject(result)) {
        return data;
    }

    // yinli looks like "丙午(马)年八月十三": keep only the part after 年 for
    // the dashboard's 农历 line.
    std::string yinli = GetStringField(result, "yinli");
    size_t nian = yinli.find("年");
    if (nian == std::string::npos || nian + 3 >= yinli.size()) {
        return data;  // Incomplete payload -> treat as request failure
    }
    data.lunar_date = yinli.substr(nian + 3);  // 年 is 3 bytes in UTF-8

    data.yi = SplitActivities(GetStringField(result, "yi"));
    data.ji = SplitActivities(GetStringField(result, "ji"));
    data.ok = true;
    return data;
}
