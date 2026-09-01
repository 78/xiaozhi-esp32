#ifndef BOARD_PROTOCOL_H
#define BOARD_PROTOCOL_H

#include <cJSON.h>

#include <functional>
#include <memory>

#include "protocol.h"

struct BoardProtocolContext {
    std::function<void(std::function<void()>&&)> schedule;
    std::function<void()> on_speech_finished;
};

class BoardProtocolHandler {
public:
    virtual ~BoardProtocolHandler() = default;
    virtual void SetApplicationContext(BoardProtocolContext context) { (void)context; }
    virtual bool HandleIncomingJson(const cJSON* root) {
        (void)root;
        return false;
    }
    virtual bool HandleIncomingAudio(std::unique_ptr<AudioStreamPacket>& packet) {
        (void)packet;
        return false;
    }
    virtual void OnAudioChannelClosed() {}
    virtual void OnAbortSpeaking() {}
    virtual void OnApplicationTick() {}
};

#endif  // BOARD_PROTOCOL_H
