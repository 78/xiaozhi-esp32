#include "notify_player.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

#include <esp_log.h>

#include "board.h"
#include "http.h"
#include "ogg_demuxer.h"

namespace {
constexpr int kHttpTimeoutMs = 5000;
constexpr size_t kHttpReadBufferSize = 1024;
constexpr uint32_t kNotifyTaskStackSize = 6144;
constexpr UBaseType_t kNotifyTaskPriority = 2;
const char* TAG = "NotifyPlayer";

bool IsSupportedUrl(const std::string& url) {
    return url.compare(0, 7, "http://") == 0 || url.compare(0, 8, "https://") == 0;
}
}  // namespace

NotifyPlayer::NotifyPlayer(AudioService& audio_service) : audio_service_(audio_service) {}

NotifyPlayer::~NotifyPlayer() { Stop(); }

bool NotifyPlayer::Start(std::string audio_url, std::vector<NotifySubtitle> subtitles,
                         uint32_t playback_id, SubtitleCallback subtitle_callback,
                         FinishedCallback finished_callback) {
    if (playback_id == 0 || !IsSupportedUrl(audio_url)) {
        return false;
    }

    std::stable_sort(subtitles.begin(), subtitles.end(),
                     [](const NotifySubtitle& left, const NotifySubtitle& right) {
                         return left.start_ms < right.start_ms;
                     });

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_ || worker_running_) {
            return false;
        }
        audio_url_ = std::move(audio_url);
        subtitles_ = std::move(subtitles);
        displayed_text_.clear();
        subtitle_callback_ = std::move(subtitle_callback);
        finished_callback_ = std::move(finished_callback);
        playback_id_ = playback_id;
        last_playback_position_ms_ = 0;
        underrun_count_ = 0;
        next_subtitle_index_ = 0;
        active_ = true;
        worker_running_ = true;
        cancelled_ = false;
        http_finished_ = false;
        stream_started_ = false;
        playback_drained_ = false;
        completion_reported_ = false;
    }

    BaseType_t created = xTaskCreate(WorkerEntry, "notify_http", kNotifyTaskStackSize, this,
                                     kNotifyTaskPriority, &task_handle_);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create notification HTTP task");
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        worker_running_ = false;
        cancelled_ = true;
        task_handle_ = nullptr;
        return false;
    }
    return true;
}

void NotifyPlayer::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
    active_ = false;
    subtitles_.clear();
    subtitle_callback_ = nullptr;
    finished_callback_ = nullptr;
}

bool NotifyPlayer::IsActive(uint32_t playback_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ && (playback_id == 0 || playback_id == playback_id_);
}

bool NotifyPlayer::IsBusy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ || worker_running_;
}

bool NotifyPlayer::IsCancelled(uint32_t playback_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancelled_ || !active_ || playback_id != playback_id_;
}

void NotifyPlayer::OnPlaybackProgress(uint32_t playback_id, uint32_t media_position_ms) {
    SubtitleCallback callback;
    std::string text;
    bool subtitle_changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || playback_id != playback_id_) {
            return;
        }
        last_playback_position_ms_ = media_position_ms;
        while (next_subtitle_index_ < subtitles_.size() &&
               subtitles_[next_subtitle_index_].start_ms <= media_position_ms) {
            text = subtitles_[next_subtitle_index_].text;
            ++next_subtitle_index_;
            subtitle_changed = true;
        }
        if (!subtitle_changed || text == displayed_text_) {
            return;
        }
        displayed_text_ = text;
        callback = subtitle_callback_;
    }
    if (callback) {
        callback(playback_id, text);
    }
}

NotifyPlayer::FinishedCallback NotifyPlayer::CompleteLocked(uint32_t& playback_id) {
    if (completion_reported_ || !active_) {
        return nullptr;
    }
    completion_reported_ = true;
    active_ = false;
    playback_id = playback_id_;
    return finished_callback_;
}

void NotifyPlayer::OnPlaybackDrained() {
    FinishedCallback callback;
    uint32_t playback_id = 0;
    uint32_t underrun_count = 0;
    uint32_t media_position_ms = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) {
            return;
        }
        playback_drained_ = true;
        if (http_finished_) {
            callback = CompleteLocked(playback_id);
        } else if (stream_started_) {
            underrun_count = ++underrun_count_;
            media_position_ms = last_playback_position_ms_;
        }
    }
    if (underrun_count != 0) {
        ESP_LOGW(TAG, "Notification playback underrun #%lu at %lu ms",
                 static_cast<unsigned long>(underrun_count),
                 static_cast<unsigned long>(media_position_ms));
    }
    if (callback) {
        callback(playback_id, true);
    }
}

void NotifyPlayer::WorkerEntry(void* arg) {
    auto* player = static_cast<NotifyPlayer*>(arg);
    player->WorkerTask();
    vTaskDelete(nullptr);
}

void NotifyPlayer::WorkerTask() {
    std::string audio_url;
    uint32_t playback_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_url = audio_url_;
        playback_id = playback_id_;
    }

    bool success = false;
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    if (http) {
        http->SetTimeout(kHttpTimeoutMs);
        http->SetHeader("Accept", "audio/ogg, application/ogg");
        http->SetHeader("Accept-Encoding", "identity");
        const bool opened = http->Open("GET", audio_url);
        if (opened) {
            const int status = http->GetStatusCode();
            if (status >= 200 && status < 300 && !IsCancelled(playback_id)) {
                auto demuxer = std::make_unique<OggDemuxer>();
                uint32_t media_position_ms = 0;
                bool packet_error = false;
                demuxer->OnPacket(
                    [this, playback_id, &media_position_ms, &packet_error](
                        const uint8_t* data, int sample_rate, int frame_duration_ms, size_t size) {
                        if (packet_error || IsCancelled(playback_id)) {
                            packet_error = true;
                            return;
                        }

                        auto packet = std::make_unique<AudioStreamPacket>();
                        packet->sample_rate = sample_rate;
                        packet->frame_duration = frame_duration_ms;
                        packet->playback_id = playback_id;
                        packet->media_position_ms = media_position_ms;
                        packet->payload.assign(data, data + size);

                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            if (cancelled_ || !active_ || playback_id != playback_id_) {
                                packet_error = true;
                                return;
                            }
                            playback_drained_ = false;
                        }

                        if (!audio_service_.PushPacketToDecodeQueue(std::move(packet), true)) {
                            packet_error = true;
                            return;
                        }
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            stream_started_ = true;
                        }
                        media_position_ms += frame_duration_ms;
                    });

                std::array<char, kHttpReadBufferSize> buffer;
                while (!packet_error && !IsCancelled(playback_id)) {
                    int size = http->Read(buffer.data(), buffer.size());
                    if (size < 0) {
                        ESP_LOGE(TAG, "Notification HTTP read failed: %d", http->GetLastError());
                        break;
                    }
                    if (size == 0) {
                        success = demuxer->Finish();
                        if (!success) {
                            ESP_LOGE(
                                TAG,
                                "Notification Ogg stream ended before a complete audio stream");
                        }
                        break;
                    }
                    demuxer->Process(reinterpret_cast<const uint8_t*>(buffer.data()), size);
                    if (demuxer->HasError()) {
                        packet_error = true;
                    }
                }
            } else {
                ESP_LOGE(TAG, "Notification HTTP request returned status %d", status);
            }
        } else {
            ESP_LOGE(TAG, "Failed to open notification HTTP request: %d", http->GetLastError());
        }
        http->Close();
        http.reset();
    }

    FinishedCallback callback;
    uint32_t completed_playback_id = 0;
    bool report_success = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        worker_running_ = false;
        task_handle_ = nullptr;
        if (!cancelled_ && active_ && playback_id == playback_id_) {
            if (!success) {
                callback = CompleteLocked(completed_playback_id);
            } else {
                http_finished_ = true;
                if (playback_drained_) {
                    callback = CompleteLocked(completed_playback_id);
                    report_success = true;
                }
            }
        }
    }
    if (callback) {
        callback(completed_playback_id, report_success);
    }
}
