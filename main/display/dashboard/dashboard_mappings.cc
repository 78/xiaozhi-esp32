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

uint32_t WeatherBadgeColor(const std::string& icon_code) {
    int code = 0;
    try {
        code = std::stoi(icon_code);
    } catch (...) {
        return 0x9E9E9E;  // unparseable code -> gray
    }
    if (code == 100 || code == 150) {
        return 0xFB8C00;  // 晴 (day/night) orange
    }
    if (code >= 101 && code <= 103) {
        return 0x7CB342;  // 多云/间晴 green
    }
    if (code == 104) {
        return 0x78909C;  // 阴 blue-gray
    }
    if (code >= 300 && code < 400) {
        if (code == 302 || code == 303 || code == 304) {
            return 0x8E24AA;  // 雷阵雨 purple
        }
        return 0x1E88E5;      // 雨 blue
    }
    if (code >= 400 && code < 500) {
        return 0x29B6F6;      // 雪 light blue
    }
    return 0x9E9E9E;          // 雾/霾/未知 gray
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

const char* WeekdayZhFull(int weekday) {
    static const char* kWeekdays[] = {"星期日", "星期一", "星期二", "星期三",
                                      "星期四", "星期五", "星期六"};
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return kWeekdays[weekday];
}
