#ifndef _FIRMWARE_UPGRADE_H
#define _FIRMWARE_UPGRADE_H

#include <string>

class FirmwareUpgrade {
public:
    FirmwareUpgrade();
    ~FirmwareUpgrade();

    void CheckVersion();
    bool HasNewVersion() { return has_new_version_; }
    void StartUpgrade() { Upgrade(new_version_url_); }
    void MarkValid();

private:
    bool has_new_version_ = false;
    std::string new_version_;
    std::string new_version_url_;

    void Upgrade(std::string firmware_url);
};

#endif // _FIRMWARE_UPGRADE_H
