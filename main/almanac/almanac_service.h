#ifndef ALMANAC_SERVICE_H
#define ALMANAC_SERVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "almanac_key_store.h"

struct AlmanacSnapshot {
    bool valid = false;
    bool no_credentials = false;
    bool credentials_invalid = false;
    std::string lunar_date;  // e.g. "八月十三"
    std::string solar_term;  // e.g. "秋分"; empty when there is no term today
    std::vector<std::string> yi;
    std::vector<std::string> ji;
    time_t updated_at = 0;
};

class AlmanacService {
public:
    static AlmanacService& GetInstance();

    void Start();
    void OnKeyUpdated();
    void OnNetworkConnected();
    void OnNetworkDisconnected();

    AlmanacSnapshot GetSnapshot();
    void SetUpdateCallback(std::function<void()> callback);

private:
    AlmanacService() = default;
    void TaskLoop();
    void Publish(const AlmanacSnapshot& snapshot);

    std::atomic<bool> started_{false};
    std::atomic<bool> network_connected_{false};
    TaskHandle_t task_ = nullptr;
    std::mutex mutex_;
    AlmanacSnapshot snapshot_;
    std::function<void()> update_callback_;
    AlmanacKeyStore key_store_;
};

#endif  // ALMANAC_SERVICE_H
