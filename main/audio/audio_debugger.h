#ifndef AUDIO_DEBUGGER_H
#define AUDIO_DEBUGGER_H

#include <cstdint>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>

class AudioDebugger {
public:
    AudioDebugger();
    ~AudioDebugger();

    void Feed(const std::vector<int16_t>& data);

private:
    int udp_sockfd_ = -1;
    struct sockaddr_in udp_server_addr_;
};

#endif
