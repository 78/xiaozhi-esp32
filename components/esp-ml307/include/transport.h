#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

#include <cstddef>

class Transport {
public:
    virtual ~Transport() = default;
    virtual bool Connect(const char* host, int port) = 0;
    virtual void Disconnect() = 0;
    virtual int Send(const char* data, size_t length) = 0;
    virtual int Receive(char* buffer, size_t bufferSize) = 0;

    bool connected() const { return connected_; }

protected:
    bool connected_ = false;
};

#endif // _TRANSPORT_H_
