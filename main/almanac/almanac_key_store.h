#ifndef ALMANAC_KEY_STORE_H
#define ALMANAC_KEY_STORE_H

#include <string>

// Juhe (聚合数据) AppKey for the 老黄历 API. Stored in NVS only, never in
// code or logs.
class AlmanacKeyStore {
public:
    std::string GetKey() const;
    // An empty key erases the NVS entry.
    void SetKey(const std::string& key);
    // Create the NVS entry with an empty value if it does not exist yet, so
    // the field is visible to external NVS editing tools on a fresh device.
    void EnsureCreated();
};

#endif  // ALMANAC_KEY_STORE_H
