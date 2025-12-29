#ifndef WEATHER_CONFIG_H
#define WEATHER_CONFIG_H

#include "sdkconfig.h"

// Weather update interval (in milliseconds)
#ifndef WEATHER_UPDATE_INTERVAL_MS
#define WEATHER_UPDATE_INTERVAL_MS (30 * 60 * 1000) // 30 minutes
#endif

// Default OpenWeatherMap API key
#ifdef CONFIG_WEATHER_API_KEY
#define OPEN_WEATHERMAP_API_KEY CONFIG_WEATHER_API_KEY
#else
#define OPEN_WEATHERMAP_API_KEY "ae8d3c2fda691593ce3e84472ef25784" // Demo key
#endif

// Default City
#ifdef CONFIG_WEATHER_CITY
#define WEATHER_CITY_DEFAULT CONFIG_WEATHER_CITY
#else
#define WEATHER_CITY_DEFAULT "Hanoi"
#endif

// API Endpoints
#define WEATHER_API_ENDPOINT "https://api.openweathermap.org/data/2.5/weather"
#define AIR_POLLUTION_API_ENDPOINT "http://api.openweathermap.org/data/2.5/air_pollution"
#define ONE_CALL_API_ENDPOINT "https://api.openweathermap.org/data/2.5/onecall" // Requires paid/subscription for 3.0, 2.5 is deprecated but some keys work
#define IP_LOCATION_API_ENDPOINT "https://ipwho.is"

// HTTP timeout
#define WEATHER_HTTP_TIMEOUT_MS 10000

#endif // WEATHER_CONFIG_H
