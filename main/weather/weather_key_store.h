#ifndef WEATHER_KEY_STORE_H
#define WEATHER_KEY_STORE_H

#include <string>

class WeatherKeyStore {
public:
    std::string GetKey() const;
    void SetKey(const std::string& key);
};

#endif  // WEATHER_KEY_STORE_H
