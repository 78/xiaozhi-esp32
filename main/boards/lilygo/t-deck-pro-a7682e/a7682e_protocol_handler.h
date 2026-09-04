#ifndef A7682E_PROTOCOL_HANDLER_H
#define A7682E_PROTOCOL_HANDLER_H

#include "a7682e_modem.h"
#include "board_protocol.h"

#include <atomic>
#include <cstdint>
#include <mutex>

class A7682eProtocolHandler final : public BoardProtocolHandler {
public:
    explicit A7682eProtocolHandler(A7682eModem& modem);
    void SetApplicationContext(BoardProtocolContext context) override;
    bool HandleIncomingJson(const cJSON* root) override;
    bool HandleIncomingAudio(std::unique_ptr<AudioStreamPacket>& packet) override;
    void OnAudioChannelClosed() override;
    void OnAbortSpeaking() override;
    void OnApplicationTick() override;

private:
    bool IsCurrentGeneration(uint32_t generation) const;
    uint32_t CancelSpeech();
    A7682eModem& modem_;
    BoardProtocolContext context_;
    std::mutex lifecycle_mutex_;
    std::atomic<uint32_t> tts_generation_{0};
    std::atomic<uint32_t> active_tts_generation_{0};
    std::atomic<bool> accepting_tts_sentences_{false};
    std::atomic<uint32_t> local_tts_drain_generation_{0};
    std::atomic<uint32_t> discarded_audio_frame_count_{0};
    std::atomic<uint32_t> discarded_audio_byte_count_{0};
};

#endif  // A7682E_PROTOCOL_HANDLER_H
