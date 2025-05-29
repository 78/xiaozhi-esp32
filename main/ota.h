#ifndef _OTA_H
#define _OTA_H

#include <functional>
#include <string>
#include <map>

class Ota {
public:
    Ota();
    ~Ota();

    void SetCheckVersionUrl(std::string check_version_url);
    void SetHeader(const std::string& key, const std::string& value);
    bool CheckVersion();
    bool HasNewVersion() { return has_new_version_; }
    bool HasMqttConfig() { return has_mqtt_config_; }
    bool HasActivationCode() { return has_activation_code_; }
    bool HasServerTime() { return has_server_time_; }
    bool HasWebsocketConfig() { return has_websocket_config_; }
    void StartUpgrade(std::function<void(int progress, size_t speed)> callback);
    void MarkCurrentVersionValid();

    const std::string& GetFirmwareVersion() const { return firmware_version_; }
    const std::string& GetCurrentVersion() const { return current_version_; }
    const std::string& GetActivationMessage() const { return activation_message_; }
    const std::string& GetActivationCode() const { return activation_code_; }
    const std::string& GetWebsocketUrl() const { return websocket_url_; }
    const std::string& GetWebsocketToken() const { return websocket_token_; }

private:
    std::string check_version_url_;
    std::string activation_message_;
    std::string activation_code_;
    bool has_new_version_ = false;
    bool has_mqtt_config_ = false;
    bool has_server_time_ = false;
    bool has_activation_code_ = false;
    bool has_websocket_config_ = false;
    std::string current_version_;
    std::string firmware_version_;
    std::string firmware_url_;
    std::string websocket_url_;
    std::string websocket_token_;
    std::map<std::string, std::string> headers_;

    void Upgrade(const std::string& firmware_url);
    std::function<void(int progress, size_t speed)> upgrade_callback_;
    std::vector<int> ParseVersion(const std::string& version);
    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);
};

#endif // _OTA_H
