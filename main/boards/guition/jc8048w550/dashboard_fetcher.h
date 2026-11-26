#ifndef JC8048W550_DASHBOARD_FETCHER_H
#define JC8048W550_DASHBOARD_FETCHER_H

#include "dashboard_model.h"

#include <cstdint>
#include <functional>
#include <string>

// Keeps a dashboard::Snapshot fresh from the network in a task of its own.
// Fetches only while the device is idle, the clock is set and enough internal
// RAM is free; a request that is already running when a conversation starts is
// left to finish (its timeout is 10 s). The listener is called from the fetch
// task, so it must take the display lock before touching LVGL.
class DashboardFetcher {
public:
    using Listener = std::function<void(const dashboard::Snapshot&)>;

    explicit DashboardFetcher(Listener on_update);
    // Creates the task; call once.
    void Start();

private:
    static void TaskEntry(void* arg);
    void Run();
    bool HttpGet(const std::string& url, std::string& body);
    bool RefreshLocation();
    bool RefreshWeather();
    bool RefreshRepos();

    Listener on_update_;
    dashboard::Snapshot snapshot_;
    dashboard::GeoLocation location_;
    bool have_location_ = false;
    int64_t weather_due_s_ = 0;  // esp_timer seconds, not wall-clock
    int64_t repos_due_s_ = 0;
};

#endif  // JC8048W550_DASHBOARD_FETCHER_H
