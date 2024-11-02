#ifndef BOARD_H
#define BOARD_H

#include "config.h"
#include <Http.h>
#include <WebSocket.h>
#include <AudioDevice.h>
#include <string>

void* create_board();

class Board {
public:
    static Board& GetInstance() {
        static Board* instance = nullptr;
        if (nullptr == instance) {
            instance = static_cast<Board*>(create_board());
        }
        return *instance;
    }

    virtual void Initialize() = 0;
    virtual void StartNetwork() = 0;
    virtual ~Board() = default;
    virtual AudioDevice* CreateAudioDevice() = 0;
    virtual Http* CreateHttp() = 0;
    virtual WebSocket* CreateWebSocket() = 0;
    virtual bool GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) = 0;
    virtual bool GetBatteryVoltage(int &voltage, bool& charging);
    virtual std::string GetJson();

protected:
    Board() = default;

private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作
    virtual std::string GetBoardJson() = 0;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
