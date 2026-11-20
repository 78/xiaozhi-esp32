#ifndef PHONE_MUSIC_H
#define PHONE_MUSIC_H

// Plays a song on the bot's own speaker from a raw audio stream served by the phone
// (see phone-mcp/local_music.py). The stream is 16-bit mono PCM at the codec's output rate.
//
// Flow: the cloud assistant calls the phone tool local_music_play(target "bot"), which returns a
// URL (host "gateway" means the Wi-Fi gateway, i.e. the phone when it is the hotspot); it then
// calls the device tool self.music.play_url with that URL. The reply finishes, the
// conversation is closed so the music is not sent to the server, and the song streams in.
// Starting a new conversation, or self.music.stop, ends the music.

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "board.h"

namespace phone_music {

static const char* const kTag = "PhoneMusic";
static std::atomic<bool> g_running{false};
static std::atomic<uint32_t> g_session{0};

struct Job {
    std::string url;
};

inline void StreamTask(void* arg) {
    std::unique_ptr<Job> job(static_cast<Job*>(arg));
    auto& app = Application::GetInstance();
    auto& audio = app.GetAudioService();
    auto& board = Board::GetInstance();
    const uint32_t session = g_session.load();
    auto cancelled = [session]() { return g_session.load() != session; };

    // 1. Let the spoken reply start and finish.
    for (int i = 0; i < 40 && !cancelled() && app.GetDeviceState() != kDeviceStateSpeaking; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    for (int i = 0; i < 300 && !cancelled(); i++) {
        if (app.GetDeviceState() != kDeviceStateSpeaking && audio.IsPlaybackIdle()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 2. End the conversation so the microphone does not send the music to the server.
    if (!cancelled() && app.GetDeviceState() != kDeviceStateIdle) {
        app.ToggleChatState();
        for (int i = 0; i < 50 && !cancelled() && app.GetDeviceState() != kDeviceStateIdle; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // 3. Stream.
    if (!cancelled()) {
        const int rate = board.GetAudioCodec()->output_sample_rate();
        std::string url = job->url;
        url += (url.find('?') == std::string::npos ? "?" : "&");
        url += "rate=" + std::to_string(rate);

        board.SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
        auto http = board.GetNetwork()->CreateHttp(0);
        bool opened = false;
        if (auto result = http->Open("GET", url); !result) {
            ESP_LOGE(kTag, "Cannot open %s: %s", url.c_str(), result.error().ToString().c_str());
        } else if (auto status = http->GetStatusCode(); !status || *status != 200) {
            ESP_LOGE(kTag, "The phone answered with an error");
            http->Close();
        } else {
            opened = true;
        }

        if (opened) {
            ESP_LOGI(kTag, "Streaming %s at %d Hz", url.c_str(), rate);
            const uint32_t generation = audio.GetPlaybackGeneration();
            constexpr size_t kChunkBytes = 4800;  // 100 ms at 24 kHz
            std::vector<uint8_t> buffer(kChunkBytes);
            size_t have = 0;
            while (!cancelled()) {
                auto read = http->Read(reinterpret_cast<char*>(buffer.data()) + have,
                                       buffer.size() - have);
                if (!read || *read == 0) {
                    break;  // end of song or the phone went away
                }
                have += *read;
                if (have == buffer.size()) {
                    std::vector<int16_t> pcm(kChunkBytes / sizeof(int16_t));
                    memcpy(pcm.data(), buffer.data(), kChunkBytes);
                    have = 0;
                    if (!audio.PushPcmToPlayback(std::move(pcm), generation)) {
                        break;  // a new conversation or a stop took over the speaker
                    }
                }
            }
            if (!cancelled() && have >= sizeof(int16_t)) {
                std::vector<int16_t> pcm(have / sizeof(int16_t));
                memcpy(pcm.data(), buffer.data(), pcm.size() * sizeof(int16_t));
                audio.PushPcmToPlayback(std::move(pcm), generation);
            }
            http->Close();
            ESP_LOGI(kTag, "Stream finished");
        }
        if (app.GetDeviceState() == kDeviceStateIdle) {
            board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        }
    }

    g_running.store(false);
    vTaskDelete(nullptr);
}

// The phone cannot always know its own address on the bot's Wi-Fi. When the Wi-Fi is the phone's
// hotspot, the phone is the Wi-Fi gateway, so a url with the host "gateway" is pointed there.
inline std::string ResolveGateway(const std::string& url) {
    const std::string marker = "http://gateway";
    if (url.rfind(marker, 0) != 0) {
        return url;
    }
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info = {};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &info) != ESP_OK || info.gw.addr == 0) {
        return "";
    }
    char ip[16] = {};
    esp_ip4addr_ntoa(&info.gw, ip, sizeof(ip));
    return "http://" + std::string(ip) + url.substr(marker.size());
}

inline void Stop() {
    g_session.fetch_add(1);
    Application::GetInstance().GetAudioService().ResetDecoder();
}

inline std::string Start(const std::string& url) {
    if (url.rfind("http://", 0) != 0) {
        return "Not started: the url must be the http address given by the phone music tool.";
    }
    const std::string resolved = ResolveGateway(url);
    if (resolved.empty()) {
        return "Not started: the bot cannot find the Wi-Fi gateway (the phone).";
    }
    Stop();
    for (int i = 0; i < 30 && g_running.load(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (g_running.load()) {
        return "Not started: the previous song is still stopping. Try again.";
    }
    g_running.store(true);
    auto* job = new Job{resolved};
    if (xTaskCreate(StreamTask, "phone_music", 8192, job, 3, nullptr) != pdPASS) {
        delete job;
        g_running.store(false);
        return "Not started: out of memory.";
    }
    return "Starting the music on the bot's speaker.";
}

}  // namespace phone_music

#endif  // PHONE_MUSIC_H
