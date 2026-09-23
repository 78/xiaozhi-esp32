#ifndef A7670E_BOARD_H
#define A7670E_BOARD_H

#include "board.h"
#include "network_interface.h"
#include <memory>
#include <string>

// 前向声明
class A7670ENetwork;

/**
 * A7670E (SIM7670X) 蜂窝板卡实现
 * 参考：https://www.waveshare.net/wiki/ESP32-S3-A7670E-4G
 * 
 * 特性：
 * - UART AT指令通信（115200波特率）
 * - GPIO33/22控制开关机（拉低开机，拉高关机）
 * - SIMCOM标准AT指令集
 * - 支持自动APN识别或手动配置
 */
class A7670EBoard : public Board {
private:
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    gpio_num_t power_pin_;
    NetworkEventCallback network_event_callback_;
    std::unique_ptr<A7670ENetwork> network_;
    
    // 网络初始化任务（在FreeRTOS任务中运行）
    void NetworkTask();
    
    // 内部事件处理
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "");

public:
    A7670EBoard(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t power_pin = GPIO_NUM_33);
    virtual ~A7670EBoard();

    // Board 接口实现
    std::string GetBoardType() override { return "a7670e"; }
    AudioCodec* GetAudioCodec() override { return nullptr; }
    NetworkInterface* GetNetwork() override;
    void StartNetwork() override;
    void SetNetworkEventCallback(NetworkEventCallback callback) override { network_event_callback_ = std::move(callback); }
    const char* GetNetworkStateIcon() override;
    void SetPowerSaveLevel(PowerSaveLevel level) override;
    std::string GetBoardJson() override;
    std::string GetDeviceStatusJson() override;
};

#endif // A7670E_BOARD_H

