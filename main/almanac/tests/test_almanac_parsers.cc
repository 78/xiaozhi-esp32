#include <cassert>
#include <string>

#include "almanac_parsers.h"
#include "solar_terms.h"

void TestParser() {
    // Juhe success
    {
        std::string body = R"({"reason":"successed","result":{)"
                           R"("id":"6110","yangli":"2026-09-23",)"
                           R"("yinli":"丙午(马)年八月十三",)"
                           R"("yi":"装修 沐浴 祭祀 馀事勿取 铺路",)"
                           R"("ji":"结婚 出行 搬新房 安床"},"error_code":0})";
        auto a = ParseAlmanacResponse(body, 200);
        assert(a.ok);
        assert(!a.credentials_invalid);
        assert(a.lunar_date == "八月十三");
        assert(a.yi.size() == 5);
        assert(a.yi[0] == "装修");
        assert(a.yi[4] == "铺路");
        assert(a.ji.size() == 4);
        assert(a.ji[2] == "搬新房");
    }

    // Wrong key -> credentials invalid
    {
        auto a = ParseAlmanacResponse(
            R"({"reason":"错误的请求KEY!!!","result":null,"error_code":10001})", 200);
        assert(!a.ok);
        assert(a.credentials_invalid);
    }

    // Non-key business error -> plain failure
    {
        auto a = ParseAlmanacResponse(
            R"({"reason":"超过次数","result":null,"error_code":10005})", 200);
        assert(!a.ok);
        assert(!a.credentials_invalid);
        assert(!a.quota_exceeded);
    }

    // No permission / banned key -> credentials invalid
    {
        auto a = ParseAlmanacResponse(
            R"({"reason":"无权限","result":null,"error_code":10002})", 200);
        assert(!a.ok);
        assert(a.credentials_invalid);
        assert(!a.quota_exceeded);
    }

    // Daily quota / test-key limit -> quota exceeded, not a key problem
    {
        auto a = ParseAlmanacResponse(
            R"({"reason":"超过次数限制","result":null,"error_code":10012})", 200);
        assert(!a.ok);
        assert(!a.credentials_invalid);
        assert(a.quota_exceeded);
    }
    {
        auto a = ParseAlmanacResponse(
            R"({"reason":"测试KEY超限","result":null,"error_code":10013})", 200);
        assert(!a.ok);
        assert(a.quota_exceeded);
    }

    // Key error via HTTP status
    {
        auto a = ParseAlmanacResponse("forbidden", 403);
        assert(!a.ok);
        assert(a.credentials_invalid);
    }

    // Missing yinli -> failure
    {
        std::string body = R"({"reason":"ok","result":{"yi":"祭祀"},"error_code":0})";
        auto a = ParseAlmanacResponse(body, 200);
        assert(!a.ok);
    }

    // Malformed body
    {
        auto a = ParseAlmanacResponse("not json", 200);
        assert(!a.ok);
    }
}

void TestSolarTerms() {
    // Every term of 2025 (known calendar dates).
    struct Expect { int m, d; const char* name; };
    const Expect all[24] = {
        {1, 5, "小寒"}, {1, 20, "大寒"}, {2, 3, "立春"}, {2, 18, "雨水"},
        {3, 5, "惊蛰"}, {3, 20, "春分"}, {4, 4, "清明"}, {4, 20, "谷雨"},
        {5, 5, "立夏"}, {5, 21, "小满"}, {6, 5, "芒种"}, {6, 21, "夏至"},
        {7, 7, "小暑"}, {7, 22, "大暑"}, {8, 7, "立秋"}, {8, 23, "处暑"},
        {9, 7, "白露"}, {9, 23, "秋分"}, {10, 8, "寒露"}, {10, 23, "霜降"},
        {11, 7, "立冬"}, {11, 22, "小雪"}, {12, 7, "大雪"}, {12, 21, "冬至"},
    };
    for (const auto& e : all) {
        assert(std::string(GetSolarTerm(2025, e.m, e.d)) == e.name);
    }

    // 2026 spot checks, including today (秋分 2026-09-23).
    assert(std::string(GetSolarTerm(2026, 4, 5)) == "清明");
    assert(std::string(GetSolarTerm(2026, 9, 23)) == "秋分");
    assert(std::string(GetSolarTerm(2026, 12, 22)) == "冬至");

    // A regular day has no term.
    assert(std::string(GetSolarTerm(2026, 9, 22)) == "");
    assert(std::string(GetSolarTerm(2026, 1, 1)) == "");

    // Leap year: after February the current year's leap day counts. Without
    // the month-aware leap term, all March-December terms of 2024 shift +1.
    assert(std::string(GetSolarTerm(2024, 3, 20)) == "春分");
    assert(std::string(GetSolarTerm(2024, 3, 21)) == "");
    assert(std::string(GetSolarTerm(2024, 4, 19)) == "谷雨");
    assert(std::string(GetSolarTerm(2024, 8, 22)) == "处暑");
    assert(std::string(GetSolarTerm(2024, 9, 22)) == "秋分");
    assert(std::string(GetSolarTerm(2024, 12, 6)) == "大雪");
    // January-February terms of a leap year must not shift.
    assert(std::string(GetSolarTerm(2024, 1, 6)) == "小寒");
    assert(std::string(GetSolarTerm(2024, 2, 19)) == "雨水");

    // Boundary years of the supported range.
    assert(std::string(GetSolarTerm(2000, 1, 6)) == "小寒");
    assert(std::string(GetSolarTerm(2000, 2, 19)) == "雨水");
    assert(std::string(GetSolarTerm(2000, 12, 21)) == "冬至");
    assert(std::string(GetSolarTerm(2099, 1, 5)) == "小寒");
    assert(std::string(GetSolarTerm(2099, 12, 21)) == "冬至");

    // Exception table: the real date carries the term, and the day where the
    // formula misplaced it is suppressed (dates verified against HKO).
    struct Exception { int y, m, wrong_d, real_d; const char* name; };
    const Exception exceptions[9] = {
        {2002, 8, 7, 8, "立秋"},   {2008, 5, 20, 21, "小满"},
        {2016, 7, 6, 7, "小暑"},   {2019, 1, 6, 5, "小寒"},
        {2021, 12, 22, 21, "冬至"}, {2026, 2, 19, 18, "雨水"},
        {2082, 1, 19, 20, "大寒"}, {2089, 10, 22, 23, "霜降"},
        {2089, 11, 6, 7, "立冬"},
    };
    for (const auto& e : exceptions) {
        assert(std::string(GetSolarTerm(e.y, e.m, e.real_d)) == e.name);
        assert(std::string(GetSolarTerm(e.y, e.m, e.wrong_d)) == "");
    }

    // Outside the supported range -> empty, never a crash.
    assert(std::string(GetSolarTerm(1999, 1, 6)) == "");
    assert(std::string(GetSolarTerm(2100, 1, 5)) == "");
}

int main() {
    TestParser();
    TestSolarTerms();
}
