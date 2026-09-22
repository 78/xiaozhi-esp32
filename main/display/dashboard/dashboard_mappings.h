#ifndef DASHBOARD_MAPPINGS_H
#define DASHBOARD_MAPPINGS_H

#include <cstdint>
#include <string>

struct AqiLevelInfo {
    const char* category;
    uint32_t color_hex;
    uint32_t text_color_hex;
};

AqiLevelInfo AqiToLevel(int aqi);
int TempToPercent(int temp);
uint32_t WeatherIconToCodepoint(const std::string& icon_code);
uint32_t WeatherIconColor(uint32_t codepoint);
void CodepointToUtf8(uint32_t codepoint, char out[5]);
const char* WeekdayZh(int weekday);

#endif  // DASHBOARD_MAPPINGS_H
