#ifndef WEATHER_KEY_STORE_H
#define WEATHER_KEY_STORE_H

#include <string>

class WeatherKeyStore {
public:
    std::string GetKey() const;
    void SetKey(const std::string& key);
    // Create the NVS entry with an empty value if it does not exist yet, so
    // the field is visible to external NVS editing tools on a fresh device.
    void EnsureCreated();
};

#endif  // WEATHER_KEY_STORE_H
