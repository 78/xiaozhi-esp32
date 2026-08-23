#ifndef LOCAL_CONTROL_PANEL_H
#define LOCAL_CONTROL_PANEL_H

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

#include <esp_http_server.h>

#include "network_controller_types.h"

struct cJSON;

struct LocalPanelSettingsUpdate {
    bool has_lan_enabled = false;
    bool lan_enabled = true;
    bool has_speaker_volume = false;
    int speaker_volume = 0;
    bool has_wifi_credentials = false;
    std::string wifi_ssid;
    std::string wifi_password;
    bool restart_requested = false;
};

class LocalControlPanel {
public:
    enum class Exposure {
        Lan,
        Maintenance,
    };

    using StatusProvider = std::function<cJSON*()>;
    using NetworkModeSetter = std::function<bool(NetworkMode)>;
    using SettingsUpdater =
        std::function<bool(const LocalPanelSettingsUpdate&, std::string& error)>;

    LocalControlPanel(StatusProvider status_provider, NetworkModeSetter mode_setter,
                      SettingsUpdater settings_updater);
    ~LocalControlPanel();

    bool Start(Exposure exposure);
    void Stop();
    bool IsRunning() const;
    bool IsMaintenanceMode() const;

    bool HasAdminPassword() const;
    void ResetAdminPassword();
    bool IsLanEnabled() const;

private:
    struct Session {
        std::string token;
        std::string csrf;
        int64_t expires_at_ms = 0;
    };

    static constexpr size_t kMaxSessions = 4;

    StatusProvider status_provider_;
    NetworkModeSetter mode_setter_;
    SettingsUpdater settings_updater_;
    mutable std::mutex mutex_;
    httpd_handle_t server_ = nullptr;
    Exposure exposure_ = Exposure::Lan;
    std::array<Session, kMaxSessions> sessions_;
    int failed_login_attempts_ = 0;
    int64_t failed_login_window_started_ms_ = 0;
    int64_t login_locked_until_ms_ = 0;

    static esp_err_t IndexHandler(httpd_req_t* req);
    static esp_err_t StatusHandler(httpd_req_t* req);
    static esp_err_t SessionHandler(httpd_req_t* req);
    static esp_err_t NetworkModeHandler(httpd_req_t* req);
    static esp_err_t SettingsHandler(httpd_req_t* req);
    static esp_err_t AdminPasswordHandler(httpd_req_t* req);

    esp_err_t HandleIndex(httpd_req_t* req);
    esp_err_t HandleStatus(httpd_req_t* req);
    esp_err_t HandleSession(httpd_req_t* req);
    esp_err_t HandleNetworkMode(httpd_req_t* req);
    esp_err_t HandleSettings(httpd_req_t* req);
    esp_err_t HandleAdminPassword(httpd_req_t* req);

    bool RegisterHandlers();
    bool Authenticate(httpd_req_t* req, bool require_csrf);
    bool CreateSession(std::string& token, std::string& csrf);
    bool VerifyPassword(const std::string& password) const;
    bool StorePassword(const std::string& password);
    bool LoginAllowed(int64_t now_ms);
    void RecordLoginResult(bool success, int64_t now_ms);
    void ClearSessionsLocked();
};

#endif  // LOCAL_CONTROL_PANEL_H
