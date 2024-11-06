#ifndef _TLS_TRANSPORT_H_
#define _TLS_TRANSPORT_H_

#include "transport.h"
#include <esp_tls.h>

class TlsTransport : public Transport {
public:
    TlsTransport();
    ~TlsTransport();

    bool Connect(const char* host, int port) override;
    void Disconnect() override;
    int Send(const char* data, size_t length) override;
    int Receive(char* buffer, size_t bufferSize) override;

private:
    esp_tls_t* tls_client_;
};

#endif // _TLS_TRANSPORT_H_