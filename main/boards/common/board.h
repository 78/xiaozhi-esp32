#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>
#include <network_interface.h>

#include "led/led.h"
#include "backlight.h"
#include "camera.h"

void* create_board();
class AudioCodec;
class Display;
class Board {
private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作

protected:
    Board();
    std::string GenerateUuid();

    // 软件生成的设备唯一标识
    std::string uuid_;

public:
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());//返回具体的板级支持包（BSP）类实例
        return *instance;
    }

    // 虚函数，派生类需实现
    virtual ~Board() = default;
    virtual std::string GetBoardType() = 0;                        //获取板类型
    virtual std::string GetUuid() { return uuid_; }                //获取设备唯一UUID
    virtual Backlight* GetBacklight() { return nullptr; }          //获取背光对象
    virtual Led* GetLed();
    virtual AudioCodec* GetAudioCodec() = 0;                       //获取音频编解码器对象
    virtual bool GetTemperature(float& esp32temp);                 //获取ESP32温度
    virtual Display* GetDisplay();                                 //获取显示对象
    virtual Camera* GetCamera();                                   //获取摄像头对象
    virtual NetworkInterface* GetNetwork() = 0;                    //获取网络接口对象
    virtual void StartNetwork() = 0;                               //启动网络
    virtual const char* GetNetworkStateIcon() = 0;                 //获取网络状态图标
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);    //获取电池电量
    virtual std::string GetJson();                                 //获取设备状态JSON
    virtual void SetPowerSaveMode(bool enabled) = 0;               //设置省电模式
    virtual std::string GetBoardJson() = 0;                        //获取板级支持包（BSP）的JSON
    virtual std::string GetDeviceStatusJson() = 0;                 //获取设备状态JSON
};
// 返回其具体的板级支持包（BSP）类实例
#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
