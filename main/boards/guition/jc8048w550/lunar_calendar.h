#ifndef JC8048W550_LUNAR_CALENDAR_H
#define JC8048W550_LUNAR_CALENDAR_H

#include <string>

// Vietnamese lunar calendar (time zone UTC+7), after Ho Ngoc Duc's algorithm.
// Pure computation: no LVGL, no ESP headers, no network.
namespace lunar {

struct LunarDate {
    int day = 0;
    int month = 0;
    int year = 0;
    bool leap = false;  // the month is a leap month (thang nhuan)

    bool operator==(const LunarDate& other) const {
        return day == other.day && month == other.month && year == other.year &&
               leap == other.leap;
    }
};

// Julian day number of a Gregorian civil date.
int JulianDay(int day, int month, int year);

LunarDate SolarToLunar(int day, int month, int year);

// "Binh Ngo" style names (UTF-8). Month 1 is Dan.
std::string YearCanChi(int lunar_year);
std::string MonthCanChi(const LunarDate& date);
std::string DayCanChi(int jdn);

// The 15-degree solar term the sun is in on that civil date (e.g. "Tiểu thử").
const char* SolarTerm(int day, int month, int year);

// Checks the above against known dates (and the events in vn_events.h); logs
// the result. Development aid only: nothing calls it in production.
bool RunLunarSelfTest();

}  // namespace lunar

#endif  // JC8048W550_LUNAR_CALENDAR_H
