#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>
#include <network_interface.h>
#include <driver/gpio.h>

#include "led/led.h"
#include "backlight.h"
#include "camera.h"
#include "assets.h"


void* create_board();
class AudioCodec;
class Display;
class RFModule;
class Board {
private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作

protected:
    Board();
    std::string GenerateUuid();

    // 软件生成的设备唯一标识
    std::string uuid_;
    
#if CONFIG_BOARD_HAS_RF_PINS
    // RF模块实例（在板子构造函数中初始化）
    RFModule* rf_module_ = nullptr;
    // 初始化RF模块（板子应在构造函数中调用，确保虚函数GetRFPinConfig()正常工作）
    void InitializeRFModule();
#endif

public:
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());
        return *instance;
    }

    virtual ~Board();
    virtual std::string GetBoardType() = 0;
    virtual std::string GetUuid() { return uuid_; }
    virtual Backlight* GetBacklight() { return nullptr; }
    virtual Led* GetLed();
    virtual AudioCodec* GetAudioCodec() = 0;
    virtual bool GetTemperature(float& esp32temp);
    virtual Display* GetDisplay();
    virtual Camera* GetCamera();
    virtual NetworkInterface* GetNetwork() = 0;
    virtual void StartNetwork() = 0;
    virtual const char* GetNetworkStateIcon() = 0;
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);
    virtual std::string GetSystemInfoJson();
    virtual void SetPowerSaveMode(bool enabled) = 0;
    virtual std::string GetBoardJson() = 0;
    virtual std::string GetDeviceStatusJson() = 0;
    virtual RFModule* GetRFModule();
    
#if CONFIG_BOARD_HAS_RF_PINS
    // 获取RF引脚配置（板子可以重写此方法提供自定义引脚）
    // 默认实现尝试使用 config.h 中的宏定义
    virtual bool GetRFPinConfig(gpio_num_t& tx_433, gpio_num_t& rx_433, 
                                 gpio_num_t& tx_315, gpio_num_t& rx_315);
#endif
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
