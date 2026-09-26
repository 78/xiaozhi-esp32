#ifndef WEATHER_CITY_STORE_H
#define WEATHER_CITY_STORE_H

#include <string>

struct CityLocation {
    bool found = false;  // A manual city is configured
    std::string city;
    double lat = 0;
    double lon = 0;
};

// Persists the city chosen by the user through a conversation. When a manual
// city exists, WeatherService uses it and skips the GeoIP lookup.
// Strored in NVS only, never in code or logs.
class WeatherCityStore {
public:
    CityLocation GetCity() const;
    void SetCity(const std::string& city, double lat, double lon);
    void Clear();
};

#endif  // WEATHER_CITY_STORE_H
