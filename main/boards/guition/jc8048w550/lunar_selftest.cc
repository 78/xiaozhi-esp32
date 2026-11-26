#include "lunar_calendar.h"

#include "vn_events.h"

#include <esp_log.h>

#include <algorithm>
#include <string>
#include <vector>

// On-device checks for the lunar calendar. Call lunar::RunLunarSelfTest() once
// from the board while working on it and read the serial log; production code
// never calls it, so the linker drops it.
namespace lunar {
namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        g_failures++;
        ESP_LOGE("lunar-selftest", "FAILED: %s", what);
    }
}

#define CHECK(cond) Check((cond), #cond)

bool Has(const std::vector<std::string>& events, const char* name) {
    return std::find(events.begin(), events.end(), name) != events.end();
}

}  // namespace

bool RunLunarSelfTest() {
    g_failures = 0;

    CHECK(SolarToLunar(17, 2, 2026) == (LunarDate{1, 1, 2026, false}));
    CHECK(SolarToLunar(29, 1, 2025) == (LunarDate{1, 1, 2025, false}));
    CHECK(SolarToLunar(25, 7, 2025) == (LunarDate{1, 6, 2025, true}));
    CHECK(SolarToLunar(8, 8, 2025) == (LunarDate{15, 6, 2025, true}));
    CHECK(SolarToLunar(1, 1, 2000) == (LunarDate{25, 11, 1999, false}));
    CHECK(SolarToLunar(25, 9, 2026) == (LunarDate{15, 8, 2026, false}));
    CHECK(SolarToLunar(26, 4, 2026) == (LunarDate{10, 3, 2026, false}));
    LunarDate eve = SolarToLunar(16, 2, 2026);
    CHECK(eve.month == 12 && eve.day >= 29);
    CHECK(SolarToLunar(31, 12, 2025).year == 2025);

    CHECK(YearCanChi(2025) == "Ất Tỵ");
    CHECK(YearCanChi(2026) == "Bính Ngọ");
    CHECK(YearCanChi(1999) == "Kỷ Mão");
    CHECK(DayCanChi(JulianDay(1, 1, 2000)) == "Mậu Ngọ");
    CHECK(MonthCanChi(LunarDate{1, 1, 2026, false}) == "Canh Dần");
    CHECK(std::string(SolarTerm(15, 7, 2026)) == "Tiểu thử");
    CHECK(std::string(SolarTerm(23, 12, 2026)) == "Đông chí");
    // The term that begins on a day (winter solstice 2026 is 03:50 on 22/12) names that day.
    CHECK(std::string(SolarTerm(22, 12, 2026)) == "Đông chí");
    CHECK(std::string(SolarTerm(21, 12, 2026)) == "Đại tuyết");

    using dashboard::EventsFor;
    CHECK(Has(EventsFor(17, 2, 2026), "Tết Nguyên Đán"));
    CHECK(Has(EventsFor(16, 2, 2026), "Giao thừa"));
    CHECK(Has(EventsFor(26, 4, 2026), "Giỗ Tổ Hùng Vương"));
    CHECK(Has(EventsFor(25, 9, 2026), "Tết Trung Thu"));
    CHECK(!Has(EventsFor(25, 9, 2026), "Rằm"));
    CHECK(Has(EventsFor(6, 9, 2025), "Vu Lan"));
    CHECK(EventsFor(8, 8, 2025).empty());  // 15th of leap month 6: no Rằm
    CHECK(Has(EventsFor(30, 4, 2026), "Giải phóng miền Nam"));
    CHECK(Has(EventsFor(2, 9, 2026), "Quốc khánh"));
    CHECK(Has(EventsFor(1, 1, 2026), "Tết Dương lịch"));
    int first_days = 0;
    int full_moons = 0;
    const int kDaysIn2026[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int month = 1; month <= 12; ++month) {
        for (int day = 1; day <= kDaysIn2026[month - 1]; ++day) {
            std::vector<std::string> events = EventsFor(day, month, 2026);
            first_days += Has(events, "Mùng 1");
            full_moons += Has(events, "Rằm");
        }
    }
    CHECK(first_days >= 11 && first_days <= 13);
    CHECK(full_moons >= 8 && full_moons <= 12);

    if (g_failures == 0) {
        ESP_LOGI("lunar-selftest", "lunar selftest OK");
    }
    return g_failures == 0;
}

}  // namespace lunar
