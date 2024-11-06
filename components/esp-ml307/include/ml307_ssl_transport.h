#ifndef ML307_SSL_TRANSPORT_H
#define ML307_SSL_TRANSPORT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include "transport.h"
#include "ml307_at_modem.h"

#include <mutex>
#include <string>

#define ML307_SSL_TRANSPORT_CONNECTED BIT0
#define ML307_SSL_TRANSPORT_DISCONNECTED BIT1
#define ML307_SSL_TRANSPORT_ERROR BIT2
#define ML307_SSL_TRANSPORT_RECEIVE BIT3
#define ML307_SSL_TRANSPORT_SEND_COMPLETE BIT4
#define ML307_SSL_TRANSPORT_INITIALIZED BIT5

#define SSL_CONNECT_TIMEOUT_MS 10000

class Ml307SslTransport : public Transport {
public:
    Ml307SslTransport(Ml307AtModem& modem, int tcp_id);
    ~Ml307SslTransport();

    bool Connect(const char* host, int port) override;
    void Disconnect() override;
    int Send(const char* data, size_t length) override;
    int Receive(char* buffer, size_t bufferSize) override;

private:
    std::mutex mutex_;
    Ml307AtModem& modem_;
    EventGroupHandle_t event_group_handle_;
    int tcp_id_ = 0;
    std::string rx_buffer_;
    std::list<CommandResponseCallback>::iterator command_callback_it_;
};

#endif // ML307_SSL_TRANSPORT_H
