#include "dashboard_model.h"

#include <esp_log.h>

// On-device checks for the JSON parsers. Call RunModelSelfTest() once from the
// board while working on the parsers and read the serial log; production code
// never calls it, so the linker drops it together with the fixtures.
// The fixtures are trimmed copies of real responses captured with curl.
namespace dashboard {
namespace {

static const char kGeoJson[] =
    R"JSON({"success":true,"type":"IPv4","country":"Viet Nam","city":"Hanoi","latitude":21.0244665,"longitude":105.8411667})JSON";
static const char kWeatherJson[] =
    R"JSON({"latitude":21.05448,"longitude":105.807106,"generationtime_ms":0.13875961303710938,"utc_offset_seconds":25200,"timezone":"Asia/Bangkok","timezone_abbreviation":"GMT+7","elevation":24.0,"current_units":{"time":"iso8601","interval":"seconds","temperature_2m":"°C","relative_humidity_2m":"%","weather_code":"wmo code"},"current":{"time":"2026-10-29T11:00","interval":900,"temperature_2m":29.2,"relative_humidity_2m":65,"weather_code":3},"daily_units":{"time":"iso8601","weather_code":"wmo code","temperature_2m_max":"°C","temperature_2m_min":"°C"},"daily":{"time":["2026-10-29","2026-10-30","2026-10-31","2026-11-01","2026-11-02","2026-11-03","2026-11-04","2026-11-05"],"weather_code":[80,81,80,2,95,51,51,55],"temperature_2m_max":[31.5,28.6,30.5,31.7,30.8,30.0,31.6,29.6],"temperature_2m_min":[23.5,23.1,23.5,25.0,24.2,23.0,24.3,24.0]}})JSON";
static const char kReposJson[] =
    R"JSON({"total_count":4197,"incomplete_results":false,"items":[{"full_name":"ChannelCooperStart/capri-ai","description":"🌃 Capri — Edgerunners-inspired AI agent for your desktop. Works with Claude Code, Codex, Antigravity & more.","language":null,"stargazers_count":386,"forks_count":64,"topics":["ai-agent","ai-agents","ai-assistants","antigravity","autamation","claude","claude-ai","claude-code"],"updated_at":"2026-10-29T02:41:32Z","owner":{"login":"ChannelCooperStart","type":"User"}},{"full_name":"JunWan666/astrbot-for-android","description":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","language":"Kotlin","stargazers_count":241,"forks_count":11,"topics":["ai","android","astrbot","chatbot","jetpack-compose","kotlin","llm","napcat"],"updated_at":"2026-10-29T03:40:39Z","owner":{"login":"JunWan666","type":"User"}},{"full_name":"Cattmonkey/ChaosIntelligence","description":"把智能体当函数，把想法写成程序。Agent-native functional language with a CLI, Web IDE, goal-driven bots and live UI. Source-available; no AI training.","language":"Python","stargazers_count":204,"forks_count":0,"topics":["agentic-ai","ai-agents","cli","developer-tools","functional-programming","generative-ui","llm","multi-agent"],"updated_at":"2026-10-29T02:53:38Z","owner":{"login":"Cattmonkey","type":"User"}},{"full_name":"ccch1mneyyy/dsh-go","description":"dsh-go — DeepSeek Harness 装进口袋：手机本机运行，也能遥控你的电脑。Android · Unofficial.","language":null,"stargazers_count":119,"forks_count":3,"topics":["ai-agent","android","android-app","coding-agent","deepseek","deepseek-harness","dsh","dsh-tui"],"updated_at":"2026-10-29T03:59:18Z","owner":{"login":"ccch1mneyyy","type":"User"}},{"full_name":"liyupi/dafeiyu-cleaner","description":"DeepSeek 蓝色大肥鱼消除计划：识别并清理 B 站（Bilibili）评论区中由 DeepSeek 大模型、AI Agent 驱动的机器人账号（大肥鱼）的 Chrome / Edge / Firefox 浏览器插件。离线规则 + DeepSeek API 精判，AI 评论检测、空间主页鉴定、鲸鱼娘一键清场。DeepSeek AI bot detector for Bilibili comments.","language":"JavaScript","stargazers_count":119,"forks_count":6,"topics":["ai","ai-agent","ai-content-detection","ai-detection","aigc","anti-spam","bilibili","bilibili-comments"],"updated_at":"2026-10-29T03:10:52Z","owner":{"login":"liyupi","type":"User"}}]})JSON";
static const char kSixReposJson[] =
    R"JSON({"total_count":99,"incomplete_results":false,"items":[{"full_name":"ChannelCooperStart/capri-ai","description":"🌃 Capri — Edgerunners-inspired AI agent for your desktop. Works with Claude Code, Codex, Antigravity & more.","language":null,"stargazers_count":386,"forks_count":64,"topics":["ai-agent","ai-agents","ai-assistants","antigravity","autamation","claude","claude-ai","claude-code"],"updated_at":"2026-10-29T02:41:32Z","owner":{"login":"ChannelCooperStart","type":"User"}},{"full_name":"JunWan666/astrbot-for-android","description":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","language":"Kotlin","stargazers_count":241,"forks_count":11,"topics":["ai","android","astrbot","chatbot","jetpack-compose","kotlin","llm","napcat"],"updated_at":"2026-10-29T03:40:39Z","owner":{"login":"JunWan666","type":"User"}},{"full_name":"Cattmonkey/ChaosIntelligence","description":"把智能体当函数，把想法写成程序。Agent-native functional language with a CLI, Web IDE, goal-driven bots and live UI. Source-available; no AI training.","language":"Python","stargazers_count":204,"forks_count":0,"topics":["agentic-ai","ai-agents","cli","developer-tools","functional-programming","generative-ui","llm","multi-agent"],"updated_at":"2026-10-29T02:53:38Z","owner":{"login":"Cattmonkey","type":"User"}},{"full_name":"ccch1mneyyy/dsh-go","description":"dsh-go — DeepSeek Harness 装进口袋：手机本机运行，也能遥控你的电脑。Android · Unofficial.","language":null,"stargazers_count":119,"forks_count":3,"topics":["ai-agent","android","android-app","coding-agent","deepseek","deepseek-harness","dsh","dsh-tui"],"updated_at":"2026-10-29T03:59:18Z","owner":{"login":"ccch1mneyyy","type":"User"}},{"full_name":"liyupi/dafeiyu-cleaner","description":"DeepSeek 蓝色大肥鱼消除计划：识别并清理 B 站（Bilibili）评论区中由 DeepSeek 大模型、AI Agent 驱动的机器人账号（大肥鱼）的 Chrome / Edge / Firefox 浏览器插件。离线规则 + DeepSeek API 精判，AI 评论检测、空间主页鉴定、鲸鱼娘一键清场。DeepSeek AI bot detector for Bilibili comments.","language":"JavaScript","stargazers_count":119,"forks_count":6,"topics":["ai","ai-agent","ai-content-detection","ai-detection","aigc","anti-spam","bilibili","bilibili-comments"],"updated_at":"2026-10-29T03:10:52Z","owner":{"login":"liyupi","type":"User"}},{"full_name":"ChannelCooperStart/capri-ai","description":"🌃 Capri — Edgerunners-inspired AI agent for your desktop. Works with Claude Code, Codex, Antigravity & more.","language":null,"stargazers_count":386,"forks_count":64,"topics":["ai-agent","ai-agents","ai-assistants","antigravity","autamation","claude","claude-ai","claude-code"],"updated_at":"2026-10-29T02:41:32Z","owner":{"login":"ChannelCooperStart","type":"User"}}]})JSON";
static const char kRateLimitJson[] =
    R"JSON({"message":"API rate limit exceeded","documentation_url":"https://docs.github.com"})JSON";
static const char kNoItemsJson[] =
    R"JSON({"total_count":0,"incomplete_results":false,"items":[]})JSON";

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        g_failures++;
        ESP_LOGE("dashboard-selftest", "FAILED: %s", what);
    }
}

#define CHECK(cond) Check((cond), #cond)

}  // namespace

bool RunModelSelfTest() {
    g_failures = 0;

    GeoLocation geo;
    CHECK(ParseGeoIp(kGeoJson, geo));
    CHECK(geo.city == "Hanoi");
    CHECK(geo.latitude > 20.9 && geo.latitude < 21.1);
    CHECK(geo.longitude > 105.7 && geo.longitude < 105.9);
    GeoLocation untouched;
    untouched.city = "keep";
    CHECK(!ParseGeoIp("", untouched));
    CHECK(!ParseGeoIp("{}", untouched));
    CHECK(!ParseGeoIp("[1]", untouched));
    CHECK(!ParseGeoIp(R"({"success":false,"message":"nope"})", untouched));
    CHECK(untouched.city == "keep");

    Weather weather;
    CHECK(ParseWeather(kWeatherJson, "Hanoi", weather));
    CHECK(weather.city == "Hanoi");
    CHECK(weather.temp_c == 29);
    CHECK(weather.humidity == 65);
    CHECK(weather.icon == WeatherIcon::kCloudy);  // WMO code 3
    CHECK(weather.condition == "Cloudy");
    CHECK(weather.high_c > weather.low_c);
    CHECK(weather.days[0].weekday == "Fri");  // 2026-10-30, the day after today
    CHECK(weather.days[6].weekday == "Thu");  // 2026-11-05
    CHECK(weather.days[0].high_c >= weather.days[0].low_c);
    Weather weather_untouched;
    weather_untouched.city = "keep";
    CHECK(!ParseWeather("", "x", weather_untouched));
    CHECK(!ParseWeather("{}", "x", weather_untouched));
    CHECK(!ParseWeather(R"({"current":{},"daily":{}})", "x", weather_untouched));
    CHECK(weather_untouched.city == "keep");

    std::vector<Repo> repos;
    CHECK(ParseRepos(kReposJson, repos));
    CHECK(repos.size() == 5);
    if (repos.size() == 5) {
        CHECK(repos[0].full_name.find('/') != std::string::npos);
        CHECK(repos[0].stars > 0);
        CHECK(repos[0].forks >= 0);
        CHECK(repos[0].updated.size() == 10 && repos[0].updated[4] == '-');
        CHECK(!repos[0].topics.empty() && repos[0].topics.size() <= 6);
        // The emoji in the description became '?' and nothing is non-ASCII.
        for (const auto& repo : repos) {
            for (unsigned char c : repo.description) {
                CHECK(c < 0x80);
            }
        }
        // A 300-char description is cut to 200.
        CHECK(repos[1].description.size() == 200);
    }
    std::vector<Repo> six;
    CHECK(ParseRepos(kSixReposJson, six));
    CHECK(six.size() == 5);
    std::vector<Repo> none = {Repo{}};
    CHECK(ParseRepos(kNoItemsJson, none));
    CHECK(none.empty());
    std::vector<Repo> repos_untouched = {Repo{}};
    CHECK(!ParseRepos(kRateLimitJson, repos_untouched));
    CHECK(!ParseRepos("", repos_untouched));
    CHECK(!ParseRepos("[1]", repos_untouched));
    CHECK(repos_untouched.size() == 1);

    if (g_failures == 0) {
        ESP_LOGI("dashboard-selftest", "dashboard self-test OK");
    }
    return g_failures == 0;
}

}  // namespace dashboard
