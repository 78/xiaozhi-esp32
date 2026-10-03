#include "weather_city_store.h"

#include "settings.h"

#include <cstdio>
#include <cstdlib>

namespace {
constexpr const char* kNamespace = "weather";
constexpr const char* kKeyCity = "manual_city";
constexpr const char* kKeyLat = "manual_lat";
constexpr const char* kKeyLon = "manual_lon";
}  // namespace

CityLocation WeatherCityStore::GetCity() const {
    Settings settings(kNamespace, false);
    std::string city = settings.GetString(kKeyCity);
    std::string lat_s = settings.GetString(kKeyLat);
    std::string lon_s = settings.GetString(kKeyLon);

    CityLocation location;
    if (!city.empty() && !lat_s.empty() && !lon_s.empty()) {
        location.found = true;
        location.city = city;
        location.lat = atof(lat_s.c_str());
        location.lon = atof(lon_s.c_str());
    }
    return location;
}

void WeatherCityStore::SetCity(const std::string& city, double lat, double lon) {
    char coord[24];
    Settings settings(kNamespace, true);
    settings.SetString(kKeyCity, city);
    snprintf(coord, sizeof(coord), "%.4f", lat);
    settings.SetString(kKeyLat, coord);
    snprintf(coord, sizeof(coord), "%.4f", lon);
    settings.SetString(kKeyLon, coord);
}

void WeatherCityStore::Clear() {
    Settings settings(kNamespace, true);
    settings.EraseKey(kKeyCity);
    settings.EraseKey(kKeyLat);
    settings.EraseKey(kKeyLon);
}
