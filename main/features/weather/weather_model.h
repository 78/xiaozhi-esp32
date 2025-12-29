#ifndef WEATHER_MODEL_H
#define WEATHER_MODEL_H

#include <string>

struct WeatherInfo
{
    std::string city;
    std::string description;
    std::string icon_code;
    float temp = 0.0f;
    int humidity = 0;
    float feels_like = 0.0f;
    int pressure = 0;
    float wind_speed = 0.0f;

    // New fields
    float uv_index = 0.0f;
    float pm2_5 = 0.0f;

    bool valid = false;
};

struct IdleCardInfo
{
    std::string city;
    std::string time_text;
    std::string date_text;       // Solar Date
    std::string lunar_date_text; // Lunar Date
    std::string can_chi_year;    // Nam At Ti
    std::string temperature_text;
    std::string humidity_text;
    std::string description_text;
    std::string uv_text;
    std::string pm25_text;
    const char *icon = nullptr;
};

#endif // WEATHER_MODEL_H
