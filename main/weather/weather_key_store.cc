#include "weather_key_store.h"

#include "settings.h"

std::string WeatherKeyStore::GetKey() const {
    Settings settings("weather", false);
    return settings.GetString("qweather_key");
}

void WeatherKeyStore::SetKey(const std::string& key) {
    Settings settings("weather", true);
    if (key.empty()) {
        settings.EraseKey("qweather_key");
    } else {
        settings.SetString("qweather_key", key);
    }
}

void WeatherKeyStore::EnsureCreated() {
    Settings settings("weather", true);
    if (!settings.HasKey("qweather_key")) {
        settings.SetString("qweather_key", "");
    }
}
