#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cJSON.h>
#include <string>
#include <functional>

class Protocol {
public:
    virtual ~Protocol() = default;

    virtual void OnIncomingAudio(std::function<void(const std::string& data)> callback) = 0;
    virtual void OnIncomingJson(std::function<void(const cJSON* root)> callback) = 0;
    virtual void SendAudio(const std::string& data) = 0;
    virtual void SendText(const std::string& text) = 0;
    virtual void SendState(const std::string& state) = 0;
    virtual void SendAbort() = 0;
    virtual bool OpenAudioChannel() = 0;
    virtual void CloseAudioChannel() = 0;
    virtual void OnAudioChannelOpened(std::function<void()> callback) = 0;
    virtual void OnAudioChannelClosed(std::function<void()> callback) = 0;
    virtual bool IsAudioChannelOpened() const = 0;
    virtual int GetServerSampleRate() const = 0;
};

#endif // PROTOCOL_H

