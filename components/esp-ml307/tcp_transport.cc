#include "tcp_transport.h"
#include <esp_log.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#define TAG "TcpTransport"

TcpTransport::TcpTransport() : fd_(-1) {}

TcpTransport::~TcpTransport() {
    if (fd_ != -1) {
        close(fd_);
    }
}

bool TcpTransport::Connect(const char* host, int port) {
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    // host is domain
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to get host by name");
        return false;
    }
    memcpy(&server_addr.sin_addr, server->h_addr, server->h_length);

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return false;
    }

    int ret = connect(fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d", host, port);
        close(fd_);
        fd_ = -1;
        return false;
    }

    connected_ = true;
    return true;
}

void TcpTransport::Disconnect() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
    connected_ = false;
}

int TcpTransport::Send(const char* data, size_t length) {
    int ret = send(fd_, data, length, 0);
    if (ret <= 0) {
        connected_ = false;
        ESP_LOGE(TAG, "Send failed: %d", ret);
    }
    return ret;
}

int TcpTransport::Receive(char* buffer, size_t bufferSize) {
    int ret = recv(fd_, buffer, bufferSize, 0);
    if (ret == 0) {
        connected_ = false;
    } else if (ret < 0) {
        ESP_LOGE(TAG, "Receive failed: %d", ret);
    }
    return ret;
}



