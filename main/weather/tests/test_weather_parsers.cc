#include <cassert>
#include <string>

#include "weather_parsers.h"

int main() {
    // GeoIP success
    {
        auto geo = ParseGeoIpResponse(
            R"({"status":"success","city":"烟台","lat":37.4638,"lon":121.4479})");
        assert(geo.ok);
        assert(geo.city == "烟台");
        assert(geo.lat > 37.46 && geo.lat < 37.47);
        assert(geo.lon > 121.44 && geo.lon < 121.45);
    }

    // GeoIP failure status
    {
        auto geo = ParseGeoIpResponse(R"({"status":"fail","message":"private range"})");
        assert(!geo.ok);
    }

    // GeoIP malformed body
    {
        auto geo = ParseGeoIpResponse("not a json");
        assert(!geo.ok);
    }

    // QWeather now success
    {
        std::string body = R"({"code":"200","now":{"obsTime":"2026-09-23T19:14Z",)"
                           R"("temp":"17","feelsLike":"16","icon":"104","text":"阴",)"
                           R"("wind360":"180","windDir":"南","windScale":"3","windSpeed":"15",)"
                           R"("humidity":"48","precip":"0.0","pressure":"1013","vis":"24",)"
                           R"("cloud":"90","dew":"10"}})";
        auto w = ParseWeatherNowResponse(body, 200);
        assert(w.ok);
        assert(!w.key_invalid);
        assert(w.text == "阴");
        assert(w.icon == "104");
        assert(w.temperature == 17);
        assert(w.humidity == 48);
    }

    // QWeather now business error code
    {
        auto w = ParseWeatherNowResponse(R"({"code":"401"})", 200);
        assert(!w.ok);
        assert(w.key_invalid);
    }

    // QWeather now HTTP 403 with empty body
    {
        auto w = ParseWeatherNowResponse("", 403);
        assert(!w.ok);
        assert(w.key_invalid);
    }

    // QWeather now malformed
    {
        auto w = ParseWeatherNowResponse("oops", 200);
        assert(!w.ok);
        assert(!w.key_invalid);
    }

    // QWeather air success
    {
        std::string body = R"({"code":"200","now":{"aqi":"57","level":"2",)"
                           R"("category":"良","pm10":"60","pm2p5":"30"}})";
        auto a = ParseAirNowResponse(body, 200);
        assert(a.ok);
        assert(a.aqi == 57);
        assert(a.category == "良");
    }

    // QWeather air invalid key
    {
        auto a = ParseAirNowResponse(R"({"code":"403"})", 200);
        assert(!a.ok);
        assert(a.key_invalid);
    }

    // QWeather air missing field
    {
        auto a = ParseAirNowResponse(R"({"code":"200","now":{}})", 200);
        assert(!a.ok);
    }

    return 0;
}
