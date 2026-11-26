#ifndef RADIO_PLAYER_H_
#define RADIO_PLAYER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

struct RadioStation {
    std::string id;
    std::string name;
    std::string url;
    std::string genre;
};

class RadioPlayer {
public:
    static RadioPlayer& GetInstance();

    bool Play(const std::string& station_or_url);
    void Stop();
    bool IsPlaying() const;
    std::string GetCurrentStation() const;
    std::vector<RadioStation> GetStationList() const;

private:
    RadioPlayer();
    ~RadioPlayer();

    void PlayerTask();
    static void TaskEntry(void* arg);
    std::string ResolveStationUrl(const std::string& input, std::string& out_name);

    std::atomic<bool> is_playing_{false};
    std::atomic<bool> stop_requested_{false};
    TaskHandle_t task_handle_ = nullptr;
    mutable std::mutex mutex_;
    std::string current_station_name_;
    std::string current_url_;
    std::vector<RadioStation> stations_;
};

class RadioController {
public:
    RadioController();
};

#endif // RADIO_PLAYER_H_
