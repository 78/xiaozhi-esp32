#include "almanac_parsers.h"

#include "cjson_utils.h"

#include <cctype>

namespace {

std::string GetStringField(cJSON* obj, const char* key) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return field != nullptr && cJSON_IsString(field) ? std::string(field->valuestring)
                                                     : std::string();
}

// Juhe joins activities with dots, e.g. "装修.沐浴.祭祀.馀事勿取.铺路".
std::vector<std::string> SplitActivities(const std::string& text) {
    std::vector<std::string> items;
    std::string current;
    auto flush = [&]() {
        if (!current.empty()) {
            items.push_back(current);
            current.clear();        }
    };
    for (char ch : text) {
        if (ch == '.') {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();
    return items;
}

// System-level Juhe error codes shared by every API. On a non-zero
// error_code, classify credential/quota problems; returns true when the
// code was handled as an error.
bool ClassifyErrorCode(cJSON* root, int http_status, AlmanacData& data) {
    cJSON* error_code = cJSON_GetObjectItem(root, "error_code");
    if (error_code == nullptr || !cJSON_IsNumber(error_code) ||
        error_code->valueint == 0) {
        return false;
    }
    int code = error_code->valueint;
    if (http_status == 401 || http_status == 403 ||
        code == 10001 || code == 10002 || code == 10003 ||
        code == 10004 || code == 10009) {
        data.credentials_invalid = true;
    } else if (code == 10012 || code == 10013) {
        data.quota_exceeded = true;
    }
    // Any other code (e.g. 217701 no data, 217702 bad parameter) is a plain
    // request failure with data.ok left false.
    return true;
}

}  // namespace

AlmanacData ParseCalendarDay(const std::string& body, int http_status) {
    AlmanacData data;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        data.credentials_invalid = (http_status == 401 || http_status == 403);
        return data;
    }
    if (ClassifyErrorCode(root.get(), http_status, data)) {
        return data;
    }

    cJSON* result = cJSON_GetObjectItem(root.get(), "result");
    if (result == nullptr || !cJSON_IsObject(result)) {
        return data;
    }
    cJSON* day = cJSON_GetObjectItem(result, "data");
    if (day == nullptr || !cJSON_IsObject(day)) {
        return data;
    }

    data.lunar_date = GetStringField(day, "lunar");
    data.festival = GetStringField(day, "holiday");
    data.yi = SplitActivities(GetStringField(day, "suit"));
    data.ji = SplitActivities(GetStringField(day, "avoid"));
    data.ok = !data.lunar_date.empty();
    return data;
}

HolidayInfo ParseCalendarMonth(const std::string& body, const std::string& today) {
    HolidayInfo info;
    CJsonUniquePtr root(cJSON_Parse(body.c_str()));
    if (root == nullptr || !cJSON_IsObject(root.get())) {
        return info;
    }

    cJSON* result = cJSON_GetObjectItem(root.get(), "result");
    if (result == nullptr || !cJSON_IsObject(result)) {
        // error_code 217701 (no festivals this month) lands here: normal.
        return info;
    }
    cJSON* month = cJSON_GetObjectItem(result, "data");
    if (month == nullptr || !cJSON_IsObject(month)) {
        return info;
    }
    cJSON* holidays = cJSON_GetObjectItem(month, "holiday_array");
    if (holidays == nullptr || !cJSON_IsArray(holidays)) {
        return info;
    }

    cJSON* festival = nullptr;
    cJSON_ArrayForEach(festival, holidays) {
        cJSON* list = cJSON_GetObjectItem(festival, "list");
        if (list == nullptr || !cJSON_IsArray(list)) {
            continue;
        }
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, list) {
            if (GetStringField(item, "date") == today) {
                info.found = true;
                info.name = GetStringField(festival, "name");
                info.desc = GetStringField(festival, "desc");
                return info;
            }
        }
    }
    return info;
}
