#include "dashboard_model.h"

#include "cjson_utils.h"

#include <cmath>
#include <cstdio>

namespace dashboard {

// Checked at compile time: a wrong table entry stops the build.
static_assert(std::string_view(Weekday3(2026, 10, 29)) == "Thu");
static_assert(std::string_view(Weekday3(2024, 2, 29)) == "Thu");
static_assert(std::string_view(Weekday3(2026, 11, 1)) == "Sun");
static_assert(WeatherIconFromWmo(0) == WeatherIcon::kSunny);
static_assert(WeatherIconFromWmo(2) == WeatherIcon::kPartlyCloudy);
static_assert(WeatherIconFromWmo(3) == WeatherIcon::kCloudy);
static_assert(WeatherIconFromWmo(45) == WeatherIcon::kFog);
static_assert(WeatherIconFromWmo(63) == WeatherIcon::kRain);
static_assert(WeatherIconFromWmo(81) == WeatherIcon::kRain);
static_assert(WeatherIconFromWmo(75) == WeatherIcon::kSnow);
static_assert(WeatherIconFromWmo(96) == WeatherIcon::kThunder);
static_assert(WeatherIconFromWmo(1234) == WeatherIcon::kCloudy);
static_assert(std::string_view(WeatherTextFromWmo(0)) == "Clear");
static_assert(std::string_view(WeatherTextFromWmo(63)) == "Rain");
static_assert(std::string_view(WeatherTextFromWmo(1234)) == "Cloudy");
static_assert(FetchAllowed(kDeviceStateIdle, true, 100000, 60000));
static_assert(!FetchAllowed(kDeviceStateListening, true, 100000, 60000));
static_assert(!FetchAllowed(kDeviceStateSpeaking, true, 100000, 60000));
static_assert(!FetchAllowed(kDeviceStateIdle, false, 100000, 60000));
static_assert(!FetchAllowed(kDeviceStateIdle, true, 50000, 60000));

static_assert(FormatStars(0) == "0");
static_assert(FormatStars(950) == "950");
static_assert(FormatStars(999) == "999");
static_assert(FormatStars(1000) == "1.0k");
static_assert(FormatStars(12400) == "12.4k");
static_assert(FormatStars(9876543) == "9876.5k");
static_assert(FormatStars(-5) == "0");
// Valid UTF-8 is kept; a stray or truncated byte becomes '?'.
static_assert(SanitizeText("caf\xC3\xA9", 20) == "caf\xC3\xA9");
static_assert(SanitizeText("\xE4\xBD\xA0\xE5\xA5\xBD", 20) == "\xE4\xBD\xA0\xE5\xA5\xBD");
static_assert(SanitizeText("a\xE4\xBD", 20) == "a??");
static_assert(SanitizeText("a\xFF" "b", 20) == "a?b");
// The limit counts characters and never cuts one in half.
static_assert(SanitizeText("\xE4\xBD\xA0\xE5\xA5\xBD", 1) == "\xE4\xBD\xA0");
// Emoji and zero-width characters have no glyph and are dropped.
static_assert(SanitizeText("a\xF0\x9F\x9A\x80" "b", 20) == "ab");
static_assert(SanitizeText("a\xE2\x80\x8D" "b", 20) == "ab");
static_assert(SanitizeText("a\nb\tc", 20) == "a b c");
static_assert(SanitizeText("abcdef", 3) == "abc");
static_assert(SanitizeText("", 5).empty());
static_assert(SanitizeText("abc", 0).empty());
// A 300-char description is cut to the limit.
static_assert(SanitizeText(std::string_view("0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"
                                             "0123456789012345678901234567890123456789"),
                            200)
                  .size() == 200);

namespace {

constexpr size_t kMaxRepos = 5;
constexpr size_t kMaxTopics = 6;

CJsonUniquePtr ParseObject(const std::string& json) {
    CJsonUniquePtr root(cJSON_ParseWithLength(json.data(), json.size()));
    if (!root || !cJSON_IsObject(root.get())) {
        return nullptr;
    }
    return root;
}

const cJSON* Field(const cJSON* object, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

// A string field, sanitized; empty when absent or null.
std::string TextField(const cJSON* object, const char* key, size_t max_chars) {
    const cJSON* item = Field(object, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
        return "";
    }
    return SanitizeText(item->valuestring, max_chars);
}

int RoundedInt(double value) { return static_cast<int>(std::lround(value)); }

// The n-th element of a numeric array, or false if it is missing or not a number.
bool NumberAt(const cJSON* array, int index, double& out) {
    const cJSON* item = cJSON_GetArrayItem(array, index);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    out = item->valuedouble;
    return true;
}

}  // namespace

bool ParseGeoIp(const std::string& json, GeoLocation& out) {
    CJsonUniquePtr root = ParseObject(json);
    if (!root) {
        return false;
    }
    if (cJSON_IsFalse(Field(root.get(), "success"))) {
        return false;
    }
    const cJSON* latitude = Field(root.get(), "latitude");
    const cJSON* longitude = Field(root.get(), "longitude");
    if (!cJSON_IsNumber(latitude) || !cJSON_IsNumber(longitude)) {
        return false;
    }
    out.latitude = latitude->valuedouble;
    out.longitude = longitude->valuedouble;
    out.city = TextField(root.get(), "city", 32);
    return true;
}

bool ParseWeather(const std::string& json, const std::string& city, Weather& out) {
    CJsonUniquePtr root = ParseObject(json);
    if (!root) {
        return false;
    }
    const cJSON* current = Field(root.get(), "current");
    const cJSON* daily = Field(root.get(), "daily");
    if (!cJSON_IsObject(current) || !cJSON_IsObject(daily)) {
        return false;
    }
    const cJSON* temperature = Field(current, "temperature_2m");
    const cJSON* humidity = Field(current, "relative_humidity_2m");
    const cJSON* code = Field(current, "weather_code");
    const cJSON* dates = Field(daily, "time");
    const cJSON* codes = Field(daily, "weather_code");
    const cJSON* highs = Field(daily, "temperature_2m_max");
    const cJSON* lows = Field(daily, "temperature_2m_min");
    if (!cJSON_IsNumber(temperature) || !cJSON_IsNumber(humidity) || !cJSON_IsNumber(code) ||
        !cJSON_IsArray(dates) || !cJSON_IsArray(codes) || !cJSON_IsArray(highs) ||
        !cJSON_IsArray(lows)) {
        return false;
    }

    Weather weather;
    weather.city = SanitizeText(city, 32);
    weather.temp_c = RoundedInt(temperature->valuedouble);
    weather.humidity = RoundedInt(humidity->valuedouble);
    weather.icon = WeatherIconFromWmo(static_cast<int>(code->valuedouble));
    weather.condition = WeatherTextFromWmo(static_cast<int>(code->valuedouble));

    // daily[0] is today, daily[1..7] are the days after it.
    double value = 0;
    if (!NumberAt(highs, 0, value)) {
        return false;
    }
    weather.high_c = RoundedInt(value);
    if (!NumberAt(lows, 0, value)) {
        return false;
    }
    weather.low_c = RoundedInt(value);
    for (size_t i = 0; i < weather.days.size(); ++i) {
        int index = static_cast<int>(i) + 1;
        const cJSON* date = cJSON_GetArrayItem(dates, index);
        int year = 0, month = 0, day = 0;
        if (!cJSON_IsString(date) || date->valuestring == nullptr ||
            sscanf(date->valuestring, "%4d-%2d-%2d", &year, &month, &day) != 3) {
            return false;
        }
        double day_code = 0;
        double high = 0;
        double low = 0;
        if (!NumberAt(codes, index, day_code) || !NumberAt(highs, index, high) ||
            !NumberAt(lows, index, low)) {
            return false;
        }
        ForecastDay& forecast = weather.days[i];
        forecast.weekday = Weekday3(year, month, day);
        forecast.icon = WeatherIconFromWmo(static_cast<int>(day_code));
        forecast.high_c = RoundedInt(high);
        forecast.low_c = RoundedInt(low);
    }
    out = std::move(weather);
    return true;
}

bool ParseRepos(const std::string& json, std::vector<Repo>& out) {
    CJsonUniquePtr root = ParseObject(json);
    if (!root) {
        return false;
    }
    const cJSON* items = Field(root.get(), "items");
    if (!cJSON_IsArray(items)) {
        return false;
    }

    std::vector<Repo> repos;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach (item, items) {
        if (repos.size() >= kMaxRepos) {
            break;
        }
        const cJSON* stars = Field(item, "stargazers_count");
        if (!cJSON_IsObject(item) || !cJSON_IsNumber(stars)) {
            continue;
        }
        Repo repo;
        repo.full_name = TextField(item, "full_name", 48);
        if (repo.full_name.empty()) {
            continue;
        }
        repo.description = TextField(item, "description", 200);
        repo.language = TextField(item, "language", 24);
        repo.updated = TextField(item, "updated_at", 10);
        repo.stars = static_cast<int>(stars->valuedouble);
        const cJSON* forks = Field(item, "forks_count");
        repo.forks = cJSON_IsNumber(forks) ? static_cast<int>(forks->valuedouble) : 0;
        const cJSON* topics = Field(item, "topics");
        const cJSON* topic = nullptr;
        cJSON_ArrayForEach (topic, topics) {
            if (repo.topics.size() >= kMaxTopics) {
                break;
            }
            if (cJSON_IsString(topic) && topic->valuestring != nullptr) {
                repo.topics.push_back(SanitizeText(topic->valuestring, 24));
            }
        }
        repos.push_back(std::move(repo));
    }
    out = std::move(repos);
    return true;
}

}  // namespace dashboard
