#ifndef CITY_CHANGE_H
#define CITY_CHANGE_H

#include <string>

struct CityChangeResult {
    bool ok = false;
    bool key_invalid = false;
    std::string message;  // User-facing (Chinese), used in the voice reply
};

// Geocode city_name through the QWeather GeoAPI, persist it as the manual
// location in NVS and trigger an immediate weather refresh. If the city is
// unchanged, no refresh is triggered.
CityChangeResult ChangeCity(const std::string& city_name);

#endif  // CITY_CHANGE_H
