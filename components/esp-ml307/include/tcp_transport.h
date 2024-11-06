#ifndef _TCP_TRANSPORT_H_
#define _TCP_TRANSPORT_H_

#include "transport.h"

class TcpTransport : public Transport {
public:
    TcpTransport();
    ~TcpTransport();

    bool Connect(const char* host, int port) override;
    void Disconnect() override;
    int Send(const char* data, size_t length) override;
    int Receive(char* buffer, size_t bufferSize) override;

private:
    int fd_;
};

#endif // _TCP_TRANSPORT_H_