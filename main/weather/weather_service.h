#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>

#include "weather_key_store.h"

struct WeatherSnapshot {
    bool valid = false;
    bool no_key = false;
    bool key_invalid = false;
    std::string city;
    std::string weather_text;
    std::string icon_code;
    int temperature = 0;
    int humidity = 0;
    int aqi = -1;
    time_t updated_at = 0;
};

class WeatherService {
public:
    static WeatherService& GetInstance();

    void Start();
    void OnKeyUpdated();
    void OnCityUpdated();
    void OnNetworkConnected();
    void OnNetworkDisconnected();

    WeatherSnapshot GetSnapshot();
    void SetUpdateCallback(std::function<void()> callback);

private:
    WeatherService() = default;
    void TaskLoop();
    void Publish(const WeatherSnapshot& snapshot);

    std::atomic<bool> started_{false};
    std::atomic<bool> network_connected_{false};
    TaskHandle_t task_ = nullptr;
    std::mutex mutex_;
    WeatherSnapshot snapshot_;
    std::function<void()> update_callback_;
    WeatherKeyStore key_store_;
};

#endif  // WEATHER_SERVICE_H
