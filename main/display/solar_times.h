#pragma once

#include <ctime>

struct SolarTimes {
    time_t sunrise = 0;
    time_t sunset = 0;
};

// Compute the sunrise and sunset (absolute UTC time_t) around the UTC day
// containing when_utc for the given geographic coordinates (degrees, east
// positive). Returns false near the polar circles where the sun does not
// rise/set on that day.
bool CalculateSolarTimes(double lat, double lon, time_t when_utc, SolarTimes* out);

// Whether the sun is above the official horizon (zenith 90.833 degrees) at
// now_utc for the given coordinates.
bool IsDaytime(double lat, double lon, time_t now_utc);
