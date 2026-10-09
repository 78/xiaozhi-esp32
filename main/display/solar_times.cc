#include "solar_times.h"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kZenithOfficial = 90.833;  // sunrise/sunset with refraction

double Deg(double rad) { return rad * 180.0 / kPi; }
double Rad(double deg) { return deg * kPi / 180.0; }

double Mod360(double x) {
    x = std::fmod(x, 360.0);
    return x < 0 ? x + 360.0 : x;
}

// Julian Day for 12:00 UTC of the UTC date in `tm` (NOAA reference instant).
double JulianDayAtNoonUtc(const struct tm& tm_utc) {
    int year = tm_utc.tm_year + 1900;
    int month = tm_utc.tm_mon + 1;
    double day = tm_utc.tm_mday + 0.5;  // 12:00 UTC
    if (month <= 2) {
        --year;
        month += 12;
    }
    int a = year / 100;
    int b = 2 - a + a / 4;
    return std::floor(365.25 * (year + 4716)) +
           std::floor(30.6001 * (month + 1)) + day + b - 1524.5;
}

// Sunrise/sunset in minutes after UTC midnight for the day, plus whether a
// single calculation is meaningful.
bool ComputeMinutes(double lat, double lon, const struct tm& tm_utc,
                    double* sunrise_min, double* sunset_min) {
    double jd = JulianDayAtNoonUtc(tm_utc);
    double t = (jd - 2451545.0) / 36525.0;

    double l0 = Mod360(280.46646 + t * (36000.76983 + 0.0003032 * t));
    double m = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    double mr = Rad(m);
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);

    double c = std::sin(mr) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
               std::sin(2.0 * mr) * (0.019993 - 0.000101 * t) +
               std::sin(3.0 * mr) * 0.000289;
    double true_lon = l0 + c;
    double omega = 125.04 - 1934.136 * t;
    double app_lon = true_lon - 0.00569 - 0.00478 * std::sin(Rad(omega));

    double sec = 21.448 - t * (46.815 + t * (0.00059 - t * 0.001813));
    double mean_eps = 23.0 + (26.0 + sec / 60.0) / 60.0;
    double eps = mean_eps + 0.00256 * std::cos(Rad(omega));
    double decl = std::asin(std::sin(Rad(eps)) * std::sin(Rad(app_lon)));

    double y = std::tan(Rad(eps) / 2.0);
    y *= y;
    double eot = 4.0 * Deg(y * std::sin(2.0 * Rad(l0)) -
                           2.0 * e * std::sin(mr) +
                           4.0 * e * y * std::sin(mr) * std::cos(2.0 * Rad(l0)) -
                           0.5 * y * y * std::sin(4.0 * Rad(l0)) -
                           1.25 * e * e * std::sin(2.0 * mr));

    double latr = Rad(lat);
    double cos_ha = (std::cos(Rad(kZenithOfficial)) -
                     std::sin(latr) * std::sin(decl)) /
                    (std::cos(latr) * std::cos(decl));
    if (cos_ha >= 1.0) {
        return false;  // Sun stays below/at the horizon edge all day
    }
    if (cos_ha <= -1.0) {
        return false;  // Polar day
    }
    double ha = Deg(std::acos(cos_ha));
    double solar_noon = 720.0 - 4.0 * lon - eot;  // minutes after UTC midnight
    *sunrise_min = solar_noon - 4.0 * ha;
    *sunset_min = solar_noon + 4.0 * ha;
    return true;
}

time_t UtcMidnight(const struct tm& tm_utc) {
    struct tm midnight = tm_utc;
    midnight.tm_hour = 0;
    midnight.tm_min = 0;
    midnight.tm_sec = 0;
    midnight.tm_isdst = 0;
    return timegm(&midnight);
}

}  // namespace

bool CalculateSolarTimes(double lat, double lon, time_t when_utc, SolarTimes* out) {
    struct tm tm_utc;
    gmtime_r(&when_utc, &tm_utc);
    double sunrise_min = 0;
    double sunset_min = 0;
    if (!ComputeMinutes(lat, lon, tm_utc, &sunrise_min, &sunset_min)) {
        return false;
    }
    time_t midnight = UtcMidnight(tm_utc);
    out->sunrise = midnight + static_cast<time_t>(std::lround(sunrise_min * 60.0));
    out->sunset = midnight + static_cast<time_t>(std::lround(sunset_min * 60.0));
    return true;
}

bool IsDaytime(double lat, double lon, time_t now_utc) {
    // The sunrise for a far-eastern location lands on the previous UTC day, so
    // test the windows for the adjacent UTC days as well.
    for (int offset = -1; offset <= 1; ++offset) {
        SolarTimes times;
        if (CalculateSolarTimes(lat, lon, now_utc + offset * 24 * 3600, &times)) {
            if (now_utc >= times.sunrise && now_utc <= times.sunset) {
                return true;
            }
        }
    }
    return false;
}
