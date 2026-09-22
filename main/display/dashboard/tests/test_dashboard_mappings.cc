#include <cassert>
#include <cstring>

#include "dashboard_mappings.h"

int main() {
    // AQI levels and boundaries
    assert(std::string(AqiToLevel(0).category) == "优");
    assert(std::string(AqiToLevel(50).category) == "优");
    assert(std::string(AqiToLevel(51).category) == "良");
    assert(std::string(AqiToLevel(100).category) == "良");
    assert(std::string(AqiToLevel(101).category) == "轻度");
    assert(std::string(AqiToLevel(150).category) == "轻度");
    assert(std::string(AqiToLevel(151).category) == "中度");
    assert(std::string(AqiToLevel(200).category) == "中度");
    assert(std::string(AqiToLevel(201).category) == "重度");
    assert(std::string(AqiToLevel(300).category) == "重度");
    assert(std::string(AqiToLevel(301).category) == "严重");
    assert(AqiToLevel(-1).color_hex == 0x9E9E9E);

    // Temperature mapping -10..40
    assert(TempToPercent(-20) == 0);
    assert(TempToPercent(-10) == 0);
    assert(TempToPercent(15) == 50);
    assert(TempToPercent(40) == 100);
    assert(TempToPercent(50) == 100);

    // QWeather icon code mapping
    assert(WeatherIconToCodepoint("100") == 0x2600);
    assert(WeatherIconToCodepoint("101") == 0x26C5);
    assert(WeatherIconToCodepoint("103") == 0x26C5);
    assert(WeatherIconToCodepoint("104") == 0x2601);
    assert(WeatherIconToCodepoint("302") == 0x26C8);
    assert(WeatherIconToCodepoint("305") == 0x2614);
    assert(WeatherIconToCodepoint("313") == 0x2614);
    assert(WeatherIconToCodepoint("400") == 0x2744);
    assert(WeatherIconToCodepoint("404") == 0x2744);
    assert(WeatherIconToCodepoint("501") == 0x2601);
    assert(WeatherIconToCodepoint("999") == 0x2601);

    // Icon colors
    assert(WeatherIconColor(0x2600) == 0xF9A825);
    assert(WeatherIconColor(0x2601) == 0x607D8B);
    assert(WeatherIconColor(0x2614) == 0x1976D2);
    assert(WeatherIconColor(0x26C8) == 0x6A1B9A);
    assert(WeatherIconColor(0x2744) == 0x4FC3F7);

    // UTF-8 encoding
    char utf8[5];
    CodepointToUtf8('A', utf8);
    assert(std::string(utf8) == "A");
    CodepointToUtf8(0x2601, utf8);
    assert(utf8[0] == static_cast<char>(0xE2) && utf8[1] == static_cast<char>(0x98) &&
           utf8[2] == static_cast<char>(0x81) && utf8[3] == '\0');
    CodepointToUtf8(0x1F321, utf8);
    assert(utf8[0] == static_cast<char>(0xF0) && utf8[3] == static_cast<char>(0xA1));

    // Weekday
    assert(std::string(WeekdayZh(0)) == "周日");
    assert(std::string(WeekdayZh(4)) == "周四");
    assert(std::string(WeekdayZh(6)) == "周六");

    return 0;
}
