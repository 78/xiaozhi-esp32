#include "almanac_key_store.h"

#include "settings.h"

std::string AlmanacKeyStore::GetKey() const {
    Settings settings("almanac", false);
    return settings.GetString("juhe_key");
}

void AlmanacKeyStore::SetKey(const std::string& key) {
    Settings settings("almanac", true);
    if (key.empty()) {
        settings.EraseKey("juhe_key");
    } else {
        settings.SetString("juhe_key", key);
    }
}

void AlmanacKeyStore::EnsureCreated() {
    Settings settings("almanac", true);
    if (!settings.HasKey("juhe_key")) {
        settings.SetString("juhe_key", "");
    }
}
