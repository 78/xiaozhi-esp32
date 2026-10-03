#ifndef WEATHER_PARSERS_H
#define WEATHER_PARSERS_H

#include <string>

struct GeoInfo {
    bool ok = false;
    std::string city;
    double lat = 0;
    double lon = 0;
};

struct WeatherNowData {
    bool ok = false;
    bool key_invalid = false;
    std::string text;
    std::string icon;
    int temperature = 0;
    int humidity = 0;
};

struct AirNowData {
    bool ok = false;
    bool key_invalid = false;
    int aqi = -1;
    std::string category;
};

struct CityLookupData {
    bool ok = false;
    bool key_invalid = false;
    GeoInfo geo;  // city/lat/lon of the first matching location
};

GeoInfo ParseGeoIpResponse(const std::string& body);
CityLookupData ParseCityLookupResponse(const std::string& body, int http_status);
WeatherNowData ParseWeatherNowResponse(const std::string& body, int http_status);
AirNowData ParseAirNowResponse(const std::string& body, int http_status);

#endif  // WEATHER_PARSERS_H
