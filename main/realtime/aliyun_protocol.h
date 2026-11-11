#ifndef ALIYUN_REALTIME_PROTOCOL_H
#define ALIYUN_REALTIME_PROTOCOL_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <web_socket.h>
#include "config.h"
#include "protocol.h"

class AliyunProtocol final : public Protocol {
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

    void OnResponseStarted(std::function<void()> callback) {
        on_response_started_ = std::move(callback);
    }
    void OnResponseDone(std::function<void()> callback) { on_response_done_ = std::move(callback); }
    void OnTranscript(std::function<void(bool user, const std::string&)> callback) {
        on_transcript_ = std::move(callback);
    }

protected:
    bool SendText(const std::string& text) override;

private:
    void HandleEvent(const char* data, size_t size);
    AliyunRealtimeConfig config_;
    std::shared_ptr<WebSocket> websocket_;
    EventGroupHandle_t ready_events_ = nullptr;
    std::atomic<bool> listening_{false};
    std::atomic<bool> manual_{true};
    std::atomic<bool> waiting_commit_{false};
    std::atomic<bool> response_active_{false};
    std::atomic<bool> trace_open_{false};
    std::atomic<uint32_t> connection_generation_{0};
    std::string response_id_;
    std::function<void()> on_response_started_;
    std::function<void()> on_response_done_;
    std::function<void(bool, const std::string&)> on_transcript_;
};

#endif
