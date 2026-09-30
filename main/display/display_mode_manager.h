#pragma once

#include <atomic>
#include <string>

enum class DisplayMode {
    kAuto,
    kLight,
    kDark,
};

// Owns the light/dark display preference and drives automatic day/night
// switching from local sunrise/sunset. Concrete theme changes always go
// through Display::SetTheme, which restyles both the chat and the standby
// dashboard.
class DisplayModeManager {
public:
    static DisplayModeManager& GetInstance();

    void Start();

    // Switch to automatic (follow sunrise/sunset) or pin light/dark.
    void SetMode(DisplayMode mode);
    DisplayMode GetMode() const;

    // Re-evaluate now; called after the location/city changes.
    void OnLocationUpdated();

    static const char* ModeName(DisplayMode mode);
    static DisplayMode ParseMode(const std::string& name, bool* ok = nullptr);

private:
    DisplayModeManager();
    void Evaluate();
    bool GetLocation(double& lat, double& lon);
    void ApplyConcrete(const std::string& theme_name);

    std::atomic<DisplayMode> mode_{DisplayMode::kAuto};
    bool started_ = false;
};
