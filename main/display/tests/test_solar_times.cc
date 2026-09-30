#include "../solar_times.h"

#include <cassert>
#include <cstdio>

static void Print(const char* label, double lat, double lon, time_t when) {
    SolarTimes t;
    bool ok = CalculateSolarTimes(lat, lon, when, &t);
    struct tm r, s;
    gmtime_r(&t.sunrise, &r);
    gmtime_r(&t.sunset, &s);
    printf("%s ok=%d riseUTC %02d:%02d setUTC %02d:%02d\n", label, ok,
           r.tm_hour, r.tm_min, s.tm_hour, s.tm_min);
}

int main() {
    // 2026-09-30 04:00 UTC
    struct tm day = {};
    day.tm_year = 2026 - 1900;
    day.tm_mon = 8;
    day.tm_mday = 30;
    time_t when = timegm(&day);

    // Yugan, Jiangxi (28.69N, 116.69E): expect rise ~22:0x UTC (prev day),
    // set ~10:0x UTC.
    SolarTimes yg;
    assert(CalculateSolarTimes(28.6917, 116.6911, when, &yg));
    assert(yg.sunrise < when);
    assert(yg.sunset > when);

    // Day/night checks around Yugan.
    struct tm local_morning = day;  // 06:00 CST = 22:00 UTC prev day
    local_morning.tm_hour = 22;
    local_morning.tm_mday = 29;
    time_t six_cst = timegm(&local_morning);

    struct tm noon_cst = day;
    noon_cst.tm_hour = 4;  // 12:00 CST
    time_t noon = timegm(&noon_cst);
    assert(IsDaytime(28.6917, 116.6911, noon));

    struct tm late_night = day;
    late_night.tm_hour = 14;  // 22:00 CST
    time_t night = timegm(&late_night);
    assert(!IsDaytime(28.6917, 116.6911, night));

    // 06:00 CST is near sunrise; print and require it stays classified night
    // before official sunrise (~06:0x). Only assert night at 05:30 CST.
    struct tm early = day;
    early.tm_hour = 21;  // 05:00 CST
    early.tm_mday = 29;
    assert(!IsDaytime(28.6917, 116.6911, timegm(&early)));

    Print("Yugan", 28.6917, 116.6911, when);
    Print("Taizhou", 28.6614, 121.4286, when);
    Print("Beijing", 39.9042, 116.4074, when);
    printf("solar_times host tests passed\n");
    (void)six_cst;
    return 0;
}
