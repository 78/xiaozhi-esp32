#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <cstddef>

class AudioManager {
public:
    static AudioManager& GetInstance();
    void Init();
    bool StartRecording();
    bool StopRecording();
    bool PlayPcm(const uint8_t* data, size_t len);
    void RegisterOnRecordingFinished(std::function<void(std::vector<uint8_t>)> cb);

private:
    AudioManager() = default;
    std::function<void(std::vector<uint8_t>)> on_recording_finished_;
};
