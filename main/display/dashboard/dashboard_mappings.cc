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

uint32_t WeatherIconToCodepoint(const std::string& icon_code) {
    int code = 0;
    try {
        code = std::stoi(icon_code);
    } catch (...) {
        return 0x2601;
    }
    if (code == 100) {
        return 0x2600;  // sunny
    }
    if (code >= 101 && code <= 103) {
        return 0x26C5;  // sun behind cloud
    }
    if (code == 104) {
        return 0x2601;  // overcast
    }
    if (code >= 302 && code <= 304) {
        return 0x26C8;  // thunder
    }
    if (code >= 300 && code < 400) {
        return 0x2614;  // rain
    }
    if (code >= 400 && code < 500) {
        return 0x2744;  // snow
    }
    return 0x2601;      // fog/haze/unknown -> cloud
}

uint32_t WeatherIconColor(uint32_t codepoint) {
    switch (codepoint) {
        case 0x2600:
            return 0xF9A825;  // sun amber
        case 0x26C5:
            return 0x90A4AE;  // partly cloudy
        case 0x2614:
            return 0x1976D2;  // rain blue
        case 0x26C8:
            return 0x6A1B9A;  // thunder purple
        case 0x2744:
            return 0x4FC3F7;  // snow light blue
        case 0x2601:
        default:
            return 0x607D8B;  // cloud blue-gray
    }
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
