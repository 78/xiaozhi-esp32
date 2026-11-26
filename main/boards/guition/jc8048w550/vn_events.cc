#include "vn_events.h"

#include "lunar_calendar.h"

namespace dashboard {
namespace {

struct LunarEvent {
    int day;
    int month;
    const char* name;
};

struct SolarEvent {
    int day;
    int month;
    const char* name;
};

constexpr LunarEvent kLunarEvents[] = {
    {1, 1, "Tết Nguyên Đán"}, {15, 1, "Tết Nguyên Tiêu"},   {10, 3, "Giỗ Tổ Hùng Vương"},
    {15, 4, "Phật Đản"},      {5, 5, "Tết Đoan Ngọ"},       {15, 7, "Vu Lan"},
    {15, 8, "Tết Trung Thu"}, {23, 12, "Ông Công Ông Táo"},
};

constexpr SolarEvent kSolarEvents[] = {
    {1, 1, "Tết Dương lịch"},
    {30, 4, "Giải phóng miền Nam"},
    {1, 5, "Quốc tế Lao động"},
    {2, 9, "Quốc khánh"},
};

bool IsLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int DaysInMonth(int month, int year) {
    constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && IsLeapYear(year) ? 29 : kDays[month - 1];
}

}  // namespace

std::vector<std::string> EventsFor(int day, int month, int year) {
    std::vector<std::string> events;
    const lunar::LunarDate date = lunar::SolarToLunar(day, month, year);

    bool named_lunar = false;
    if (!date.leap) {
        for (const LunarEvent& event : kLunarEvents) {
            if (event.day == date.day && event.month == date.month) {
                events.push_back(event.name);
                named_lunar = true;
            }
        }
        if (date.month == 12 && date.day >= 29) {
            int next_day = day + 1;
            int next_month = month;
            int next_year = year;
            if (next_day > DaysInMonth(month, year)) {
                next_day = 1;
                next_month = month % 12 + 1;
                next_year = month == 12 ? year + 1 : year;
            }
            const lunar::LunarDate next = lunar::SolarToLunar(next_day, next_month, next_year);
            if (next.month == 1 && next.day == 1) {
                events.push_back("Giao thừa");
            }
        }
    }
    for (const SolarEvent& event : kSolarEvents) {
        if (event.day == day && event.month == month) {
            events.push_back(event.name);
        }
    }
    if (!date.leap && !named_lunar) {
        if (date.day == 1) {
            events.push_back("Mùng 1");
        } else if (date.day == 15) {
            events.push_back("Rằm");
        }
    }
    return events;
}

}  // namespace dashboard
