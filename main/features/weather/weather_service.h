#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "weather_model.h"
#include <string>
#include <mutex>
#include <atomic>

class WeatherService
{
public:
    static WeatherService &GetInstance()
    {
        static WeatherService instance;
        return instance;
    }

    bool FetchWeatherData();
    WeatherInfo GetWeatherInfo();
    bool NeedsUpdate() const;
    bool IsFetching() const { return is_fetching_; }

private:
    WeatherService();
    ~WeatherService() = default;

    bool FetchCurrentWeather(const std::string &city, const std::string &api_key);
    bool FetchAirPollution(float lat, float lon, const std::string &api_key);

    WeatherInfo weather_info_;
    mutable std::mutex mutex_;
    std::atomic<bool> is_fetching_{false};

    std::string api_key_;
    std::string city_;
    uint32_t last_update_time_;
    float lat_ = 0.0f;
    float lon_ = 0.0f;

    static std::string UrlEncode(const std::string &value);
    static std::string GetCityFromIP();
};

#endif // WEATHER_SERVICE_H
