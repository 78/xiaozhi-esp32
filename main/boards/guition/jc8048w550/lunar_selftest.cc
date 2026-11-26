#include "lunar_calendar.h"

#include <esp_log.h>

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

    if (g_failures == 0) {
        ESP_LOGI("lunar-selftest", "lunar selftest OK");
    }
    return g_failures == 0;
}

}  // namespace lunar
