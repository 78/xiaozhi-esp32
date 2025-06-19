#ifndef _DNS_SERVER_H_
#define _DNS_SERVER_H_

#include <string>
#include <esp_netif_ip_addr.h>

class DnsServer {
public:
    DnsServer();
    ~DnsServer();

    void Start(esp_ip4_addr_t gateway);
    void Stop();

private:
    int port_ = 53;
    int fd_ = -1;
    esp_ip4_addr_t gateway_;
    void Run();
};

#endif // _DNS_SERVER_H_
