#ifndef ALIYUN_REALTIME_PROTOCOL_H
#define ALIYUN_REALTIME_PROTOCOL_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <web_socket.h>
#include "config.h"
#include "protocol.h"
#include "response_tracker.h"

class AliyunProtocol final : public Protocol, public std::enable_shared_from_this<AliyunProtocol> {
public:
    explicit AliyunProtocol(AliyunRealtimeConfig config);
    ~AliyunProtocol() override;

    bool Start() override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    void SendStartListening(ListeningMode mode) override;
    void SendStopListening() override;
    void SendAbortSpeaking(AbortReason reason) override;
    void SendWakeWordDetected(const std::string& wake_word) override;
    void SendMcpMessage(const std::string& message) override;

    void OnResponseStarted(std::function<void(uint64_t)> callback) {
        on_response_started_ = std::move(callback);
    }
    void OnResponseDone(std::function<void(uint64_t)> callback) {
        on_response_done_ = std::move(callback);
    }
    bool IsCurrentResponse(uint64_t token) const;
    uint32_t connection_generation() const { return connection_generation_.load(); }
    void OnTranscript(std::function<void(bool user, const std::string&)> callback) {
        on_transcript_ = std::move(callback);
    }

protected:
    bool SendText(const std::string& text) override;

private:
    void HandleEvent(const char* data, size_t size, uint32_t generation);
    uint64_t ResponseToken() const;
    void CancelResponse(uint64_t token);
    AliyunRealtimeConfig config_;
    std::shared_ptr<WebSocket> websocket_;
    EventGroupHandle_t ready_events_ = nullptr;
    std::atomic<bool> listening_{false};
    std::atomic<bool> manual_{true};
    std::atomic<bool> waiting_commit_{false};
    std::atomic<bool> trace_open_{false};
    std::atomic<uint32_t> connection_generation_{0};
    mutable std::mutex response_mutex_;
    RealtimeResponseTracker response_;
    std::string cancel_event_id_;
    std::function<void(uint64_t)> on_response_started_;
    std::function<void(uint64_t)> on_response_done_;
    std::function<void(bool, const std::string&)> on_transcript_;
};

#endif
