#ifndef LUNAR_CALENDAR_H
#define LUNAR_CALENDAR_H

#include <cstdint>
#include <string>

struct LunarDate
{
    int day;
    int month;
    int year;
    bool is_leap;
};

class LunarCalendar
{
public:
    static LunarDate SolarToLunar(int solar_day, int solar_month, int solar_year);
    static std::string GetLunarDateString(int solar_day, int solar_month, int solar_year);
    static std::string GetCanChiYear(int lunar_year);
    static std::string GetCanChiMonth(int lunar_year, int lunar_month);
    static std::string GetCanChiDay(int solar_day, int solar_month, int solar_year); // Simplified
};

#endif // LUNAR_CALENDAR_H
