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
    void SetPostData(const std::string& post_data);
    bool CheckVersion();
    bool HasNewVersion() { return has_new_version_; }
    bool HasMqttConfig() { return has_mqtt_config_; }
    void StartUpgrade(std::function<void(int progress, size_t speed)> callback);
    void MarkCurrentVersionValid();

private:
    std::string check_version_url_;
    bool has_new_version_ = false;
    bool has_mqtt_config_ = false;
    std::string firmware_version_;
    std::string firmware_url_;
    std::string post_data_;
    std::map<std::string, std::string> headers_;

    void Upgrade(const std::string& firmware_url);
    std::function<void(int progress, size_t speed)> upgrade_callback_;
    std::vector<int> ParseVersion(const std::string& version);
    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);
};

#endif // _OTA_H
