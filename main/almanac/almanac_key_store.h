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
};

#endif  // ALMANAC_KEY_STORE_H
