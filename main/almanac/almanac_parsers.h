#ifndef ALMANAC_PARSERS_H
#define ALMANAC_PARSERS_H

#include <string>
#include <vector>

struct AlmanacData {
    bool ok = false;
    bool credentials_invalid = false;
    bool quota_exceeded = false;  // Juhe codes 10012/10013: daily limit hit
    std::string lunar_date;  // e.g. "八月十三"
    // Traditional festival from the day response's holiday field, e.g.
    // "中秋节"; empty on ordinary days (the field is null/absent then).
    std::string festival;
    std::vector<std::string> yi;
    std::vector<std::string> ji;
};

struct HolidayInfo {
    bool found = false;
    std::string name;  // e.g. "国庆节"
    std::string desc;  // arrangement notice, e.g. "10月1日至7日放假调休..."
};

// Parse the Juhe (聚合数据) 万年历 day response:
// GET https://v.juhe.cn/calendar/day?date=YYYY-M-D&key=...
AlmanacData ParseCalendarDay(const std::string& body, int http_status);

// Parse the 万年历 month response and find the festival whose date list
// (rest days + makeup workdays) contains `today`:
// GET https://v.juhe.cn/calendar/month?year-month=YYYY-M&key=...
// A month without festivals (service code 217701) simply yields found=false.
HolidayInfo ParseCalendarMonth(const std::string& body, const std::string& today);

#endif  // ALMANAC_PARSERS_H
