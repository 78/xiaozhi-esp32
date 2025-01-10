#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>

#include "led/led.h"

void* create_board();
class AudioCodec;
class Display;
class Board {
private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作
    virtual std::string GetBoardJson() = 0;

protected:
    Board();

public:
    static Board& GetInstance() {
        static Board* instance = nullptr;
        if (nullptr == instance) {
            instance = static_cast<Board*>(create_board());
        }
        return *instance;
    }

    virtual void StartNetwork() = 0;
    virtual ~Board() = default;
    virtual Led* GetLed();
    virtual AudioCodec* GetAudioCodec() = 0;
    virtual Display* GetDisplay();
    virtual Http* CreateHttp() = 0;
    virtual WebSocket* CreateWebSocket() = 0;
    virtual Mqtt* CreateMqtt() = 0;
    virtual Udp* CreateUdp() = 0;
    virtual bool GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) = 0;
    virtual const char* GetNetworkStateIcon() = 0;
    virtual bool GetBatteryLevel(int &level, bool& charging);
    virtual std::string GetJson();
    virtual void SetPowerSaveMode(bool enabled) = 0;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
