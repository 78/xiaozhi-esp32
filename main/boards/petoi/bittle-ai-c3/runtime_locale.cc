#include "sdkconfig.h"
#if CONFIG_PETOI_RUNTIME_LOCALE
#include "runtime_locale.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <network_interface.h>
#include <nvs.h>
#include <wifi_manager.h>
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "petoi_locale_assets.h"
#include "settings.h"

namespace petoi {
namespace {
constexpr const char* kTag = "PetoiLocale";
constexpr size_t kMaximumBody = 512;
#if CONFIG_PETOI_LOCALE_TEST_ENGLISH
constexpr bool kDiagnosticEnglish = true;
#else
constexpr bool kDiagnosticEnglish = false;
#endif

bool StoreLocale(const char* code, const std::string& ssid) {
    nvs_handle_t handle = 0;
    if (nvs_open("language", NVS_READWRITE, &handle) != ESP_OK)
        return false;
    // Invalidate the SSID marker first. An interrupted/failed write must not
    // associate a newly selected language with the previous network.
    auto error = nvs_erase_key(handle, "last_ssid");
    if (error == ESP_ERR_NVS_NOT_FOUND)
        error = ESP_OK;
    if (error == ESP_OK)
        error = nvs_set_str(handle, "code", code);
    if (error == ESP_OK)
        error = nvs_set_str(handle, "last_ssid", ssid.c_str());
    if (error == ESP_OK)
        error = nvs_commit(handle);
    if (error != ESP_OK) {
        nvs_erase_key(handle, "last_ssid");
        nvs_commit(handle);
    }
    nvs_close(handle);
    return error == ESP_OK;
}

std::string ReadCountry(NetworkInterface* network) {
    if (!network)
        return {};
    auto http = network->CreateHttp(0);
    if (!http)
        return {};
    struct CloseHttp {
        Http* http;
        ~CloseHttp() { http->Close(); }
    } close{http.get()};
    http->SetKeepAlive(false);
    http->SetTimeout(3000);
    const int64_t deadline = esp_timer_get_time() + 3000000;
    // Preserve the original provider; request no location detail, and never
    // transmit SSIDs, credentials, device identifiers or cached settings.
    if (!http->Open("GET", "http://ip-api.com/json/?fields=status,countryCode"))
        return {};
    const auto status = http->GetStatusCode();
    if (!status || *status != 200 || http->GetBodyLength() > kMaximumBody)
        return {};
    std::string body;
    std::array<char, 128> buffer;
    bool complete = false;
    while (esp_timer_get_time() < deadline) {
        const auto remaining = (deadline - esp_timer_get_time()) / 1000;
        if (remaining <= 0)
            return {};
        http->SetTimeout(static_cast<int>(remaining));
        const auto count = http->Read(buffer.data(), buffer.size());
        if (!count || *count < 0 || static_cast<size_t>(*count) > buffer.size())
            return {};
        if (*count == 0) {
            complete = true;
            break;
        }
        if (body.size() + static_cast<size_t>(*count) > kMaximumBody)
            return {};
        body.append(buffer.data(), *count);
    }
    if (!complete || body.empty())
        return {};
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(
        cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true), cJSON_Delete);
    if (!json)
        return {};
    auto result = cJSON_GetObjectItemCaseSensitive(json.get(), "status");
    auto country = cJSON_GetObjectItemCaseSensitive(json.get(), "countryCode");
    if (!cJSON_IsString(result) || !result->valuestring ||
        std::strcmp(result->valuestring, "success") != 0 || !cJSON_IsString(country) ||
        !country->valuestring || std::strlen(country->valuestring) != 2)
        return {};
    return country->valuestring;
}
}  // namespace

RuntimeLocale::RuntimeLocale() {
    if (kDiagnosticEnglish) {
        locale_.store(ProductLocale::English);
        return;
    }
    Settings settings("language");
    locale_.store(CachedLocale(settings.GetString("code"), ProductLocale::Chinese));
}

void RuntimeLocale::Prepare(NetworkInterface* network) {
    if (kDiagnosticEnglish) {
        ESP_LOGI(kTag, "Diagnostic locale en-US; persistent locale cache unchanged");
        return;
    }
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsConnected())
        return;
    const auto ssid = wifi.GetSsid();
    std::string code, cached_ssid;
    {
        Settings settings("language");
        code = settings.GetString("code");
        cached_ssid = settings.GetString("last_ssid");
    }
    if (!NeedsCountryLookup(code, cached_ssid, ssid)) {
        locale_.store(CachedLocale(code, ProductLocale::Chinese));
        ESP_LOGI(kTag, "Using cached locale %s", Code());
        return;
    }
    const auto country = ReadCountry(network);
    // Never cache a result for a network that disappeared during the request.
    if (!wifi.IsConnected() || wifi.GetSsid() != ssid)
        return;
    const auto decision = DecideLocale(code, cached_ssid, ssid, country, ProductLocale::Chinese);
    if (!decision.persist) {
        ESP_LOGW(kTag, "Country lookup unavailable; keep locale %s", Code());
        return;
    }
    const bool persisted = StoreLocale(LocaleCode(decision.locale), ssid);
    locale_.store(decision.locale);
    if (!persisted)
        ESP_LOGW(kTag, "Locale cache unavailable; selection is valid for this boot only");
    ESP_LOGI(kTag, "Selected startup locale %s", Code());
}

const char* RuntimeLocale::Code() const { return LocaleCode(locale_.load()); }

const char* RuntimeLocale::WakeModelHint() const {
    return locale_.load() == ProductLocale::Chinese ? "nihaoxiaozhi" : "hijason";
}

const char* RuntimeLocale::Text(const char* text) const {
    if (!text || locale_.load() == ProductLocale::Chinese)
        return text;
    for (const auto& pair : locale_assets::kTextPairs)
        if (text == pair.source || std::strcmp(text, pair.source) == 0)
            return pair.english;
    return text;
}

std::string_view RuntimeLocale::Sound(std::string_view sound) const {
    if (locale_.load() == ProductLocale::Chinese)
        return sound;
    for (const auto& pair : locale_assets::kSoundPairs)
        if (sound.data() == pair.source.data() && sound.size() == pair.source.size())
            return pair.english;
    return sound;
}
}  // namespace petoi
#endif
