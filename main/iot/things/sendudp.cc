#include "iot/thing.h"
#include <esp_log.h>
#include "lwip/sockets.h"


#define TAG "SendUDP"
#define UDP_SERVER_IP "192.168.5.59" // 修改为目标服务器的IP地址
#define UDP_SERVER_PORT 38123         // 修改为目标服务器的端口号

namespace iot {

// 这里仅定义 Lamp 的属性和方法，不包含具体的实现
class SendUDP : public Thing {
private:

    // 发送 UDP 数据
    void send_udp_data() {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(UDP_SERVER_PORT);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return;
        }

        char message[32];
        snprintf(message, sizeof(message), "Hello from ESP32-S3 %d", (int)rand()*1000);
        int err = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Message sent: %s", message);

//            Settings settings("audio", true);
        
        }

        close(sock);
    }


public:
SendUDP() : Thing("SendUDP", "发送UDP数据") {

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SendUDPData", "发送UDP数据", ParameterList(), [this](const ParameterList& parameters) {
            send_udp_data();

        });
    }
};

} // namespace iot

DECLARE_THING(SendUDP);
