#include "solar_terms.h"

#include <cmath>
#include <cstring>

namespace {

// C constants of the 寿星公式 for the 21st century (2000-2099), two terms per
// month in chronological order. All 2400 computed term days were checked
// against a VSOP87/IAU-1980 high-precision solar-longitude model.
constexpr double kC[12][2] = {
    {5.4055, 20.12},     // 小寒, 大寒
    {3.87, 18.73},       // 立春, 雨水
    {5.63, 20.646},      // 惊蛰, 春分
    {4.81, 20.1},        // 清明, 谷雨
    {5.52, 21.04},       // 立夏, 小满
    {5.678, 21.37},      // 芒种, 夏至
    {7.108, 22.83},      // 小暑, 大暑
    {7.5, 23.13},        // 立秋, 处暑
    {7.646, 23.042},     // 白露, 秋分
    {8.318, 23.438},     // 寒露, 霜降
    {7.438, 22.36},      // 立冬, 小雪
    {7.18, 21.94},       // 大雪, 冬至
};

const char* kNames[12][2] = {
    {"小寒", "大寒"}, {"立春", "雨水"}, {"惊蛰", "春分"},
    {"清明", "谷雨"}, {"立夏", "小满"}, {"芒种", "夏至"},
    {"小暑", "大暑"}, {"立秋", "处暑"}, {"白露", "秋分"},
    {"寒露", "霜降"}, {"立冬", "小雪"}, {"大雪", "冬至"},
};

// 寿星公式: day = floor(Y*0.2422 + C) - leap, Y = year mod 100.
// The leap term counts Feb 29 days already passed: for Jan-Feb terms the
// current year's leap day is still in the future, so count leap years
// before Y; from March on count Y itself. The floor must be mathematical
// (Y=0 -> -1), not C++ truncation toward zero.
int TermDay(int year, int month, double c) {
    int y = year % 100;
    int leap_base = month <= 2 ? y - 1 : y;
    int leap = leap_base >= 0 ? leap_base / 4 : (leap_base - 3) / 4;
    return static_cast<int>(std::floor(y * 0.2422 + c)) - leap;
}

// Days where the 寿星公式 is off by one. Found by comparing every term day of
// 2000-2099 with a VSOP87-truncated + IAU-1980 nutation apparent-sun-longitude
// computation (Meeus ch.25, Table 25.9), itself validated against Hong Kong
// Observatory published term times for 2019/2021/2026 (within 37 seconds).
struct Exception {
    int year;
    int month;
    int day;
    const char* name;
};

constexpr Exception kExceptions[] = {
    {2002, 8, 8, "立秋"},    {2008, 5, 21, "小满"},  {2016, 7, 7, "小暑"},
    {2019, 1, 5, "小寒"},   {2021, 12, 21, "冬至"}, {2026, 2, 18, "雨水"},
    {2082, 1, 20, "大寒"},  {2089, 10, 23, "霜降"}, {2089, 11, 7, "立冬"},
};

}  // namespace

const char* GetSolarTerm(int year, int month, int day) {
    if (year < 2000 || year > 2099 || month < 1 || month > 12) {
        return "";
    }
    for (const auto& e : kExceptions) {
        if (e.year == year && e.month == month && e.day == day) {
            return e.name;
        }
    }
    for (int i = 0; i < 2; ++i) {
        const char* name = kNames[month - 1][i];
        if (TermDay(year, month, kC[month - 1][i]) != day) {
            continue;
        }
        // The formula misplaced this term onto today: its real date this year
        // is the exception entry, so suppress the one-day-early/late result.
        for (const auto& e : kExceptions) {
            if (e.year == year && std::strcmp(e.name, name) == 0 &&
                !(e.month == month && e.day == day)) {
                return "";
            }
        }
        return name;
    }
    return "";
}
