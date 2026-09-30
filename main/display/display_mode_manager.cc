#include "display_mode_manager.h"

#include "application.h"
#include "board.h"
#include "settings.h"
#include "solar_times.h"
#include "weather_city_store.h"
#include "lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstdlib>

#define TAG "DisplayMode"

namespace {
constexpr uint64_t kCheckIntervalUs = 60 * 1000 * 1000;
}

DisplayModeManager& DisplayModeManager::GetInstance() {
    static DisplayModeManager instance;
    return instance;
}

DisplayModeManager::DisplayModeManager() {
    Settings settings("display", false);
    mode_ = ParseMode(settings.GetString("mode", "auto"));
}

void DisplayModeManager::Start() {
    if (started_) {
        return;
    }
    started_ = true;

    esp_timer_handle_t timer = nullptr;
    esp_timer_create_args_t args = {
        .callback =
            [](void* arg) {
                auto* self = static_cast<DisplayModeManager*>(arg);
                Application::GetInstance().Schedule([self]() { self->Evaluate(); });
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "display_mode",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, kCheckIntervalUs);

    Application::GetInstance().Schedule([this]() { Evaluate(); });
}

void DisplayModeManager::SetMode(DisplayMode mode) {
    mode_ = mode;
    Settings settings("display", true);
    settings.SetString("mode", ModeName(mode));

    if (mode == DisplayMode::kAuto) {
        Application::GetInstance().Schedule([this]() { Evaluate(); });
    } else {
        ApplyConcrete(mode == DisplayMode::kLight ? "light" : "dark");
    }
}

DisplayMode DisplayModeManager::GetMode() const { return mode_; }

void DisplayModeManager::OnLocationUpdated() {
    Application::GetInstance().Schedule([this]() { Evaluate(); });
}

const char* DisplayModeManager::ModeName(DisplayMode mode) {
    switch (mode) {
        case DisplayMode::kAuto: return "auto";
        case DisplayMode::kLight: return "light";
        case DisplayMode::kDark: return "dark";
    }
    return "auto";
}

DisplayMode DisplayModeManager::ParseMode(const std::string& name, bool* ok) {
    if (ok != nullptr) {
        *ok = true;
    }
    if (name == "light") {
        return DisplayMode::kLight;
    }
    if (name == "dark") {
        return DisplayMode::kDark;
    }
    if (!name.empty() && name != "auto" && ok != nullptr) {
        *ok = false;
    }
    return DisplayMode::kAuto;
}

bool DisplayModeManager::GetLocation(double& lat, double& lon) {
    CityLocation manual = WeatherCityStore().GetCity();
    if (manual.found) {
        lat = manual.lat;
        lon = manual.lon;
        return true;
    }

    Settings settings("weather", false);
    std::string lat_s = settings.GetString("geo_lat");
    std::string lon_s = settings.GetString("geo_lon");
    if (!lat_s.empty() && !lon_s.empty()) {
        lat = atof(lat_s.c_str());
        lon = atof(lon_s.c_str());
        return true;
    }

    // No resolved location yet: fall back to Taizhou until a city is chosen.
    lat = 28.6614;
    lon = 121.4286;
    return true;
}

void DisplayModeManager::ApplyConcrete(const std::string& theme_name) {
    auto* theme = LvglThemeManager::GetInstance().GetTheme(theme_name);
    auto* display = Board::GetInstance().GetDisplay();
    if (theme != nullptr && display != nullptr) {
        display->SetTheme(theme);
        ESP_LOGI(TAG, "Theme applied: %s", theme_name.c_str());
    }
}

void DisplayModeManager::Evaluate() {
    if (mode_ != DisplayMode::kAuto) {
        return;
    }
    double lat = 0;
    double lon = 0;
    if (!GetLocation(lat, lon)) {
        return;
    }

    time_t now = time(nullptr);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    // Wait until SNTP has set a real clock; a pre-sync epoch value would
    // produce a bogus day/night result and a theme flicker.
    if (tm_now.tm_year < 2025 - 1900) {
        return;
    }

    bool daytime = IsDaytime(lat, lon, now);
    std::string wanted = daytime ? "light" : "dark";
    auto* display = Board::GetInstance().GetDisplay();
    if (display == nullptr || display->GetTheme() == nullptr) {
        return;
    }
    if (wanted != display->GetTheme()->name()) {
        ApplyConcrete(wanted);
    }
}
