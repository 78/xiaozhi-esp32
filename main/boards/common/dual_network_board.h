#ifndef DUAL_NETWORK_BOARD_H
#define DUAL_NETWORK_BOARD_H

#include "board.h"
#include "wifi_board.h"
#include "a7670e_board.h"
#include "ml307_board.h"
#include <memory>

// 前向声明，避免条件编译
class A7670EBoard;
class Ml307Board;

//enum NetworkType
enum class NetworkType {
    WIFI,
    CELLULAR  // 统一为CELLULAR，支持A7670E或ML307
};

// 4G模块类型枚举（由板子CC文件在构造时指定，不需要menuconfig配置）
enum class CellularModuleType {
    A7670E,  // A7670E/SIM7670X (SIMCOM)
    ML307    // ML307 (Quectel)
};

// 双网络板卡类，可以在WiFi和4G之间切换（如果启用4G模块）
class DualNetworkBoard : public Board {
private:
    // 使用基类指针存储当前活动的板卡
    std::unique_ptr<Board> current_board_;
    NetworkType network_type_ = NetworkType::CELLULAR;  // 默认4G

    // 统一的4G模块引脚配置（A7670E和ML307共用）
    // A7670E: tx_pin, rx_pin, power_pin (power_pin用于开关机控制)
    // ML307: tx_pin, rx_pin, dtr_pin (dtr_pin用于DTR控制)
    gpio_num_t cellular_tx_pin_;
    gpio_num_t cellular_rx_pin_;
    gpio_num_t cellular_aux_pin_;  // A7670E使用为power_pin，ML307使用为dtr_pin
    CellularModuleType cellular_module_type_;  // 4G模块类型（由板子CC文件指定）

    
    // 从Settings加载网络类型
    NetworkType LoadNetworkTypeFromSettings(int32_t default_net_type);
    
    // 保存网络类型到Settings
    void SaveNetworkTypeToSettings(NetworkType type);

    // 初始化当前网络类型对应的板卡
    void InitializeCurrentBoard();
 
public:
    // 统一的构造函数：A7670E和ML307共用
    // tx_pin: UART TX引脚
    // rx_pin: UART RX引脚
    // aux_pin: A7670E使用为power_pin（开关机控制），ML307使用为dtr_pin（DTR控制）
    // module_type: 4G模块类型（A7670E或ML307），由板子CC文件根据硬件指定
    DualNetworkBoard(gpio_num_t tx_pin, gpio_num_t rx_pin,                     
                     gpio_num_t aux_pin = GPIO_NUM_NC,
                     int32_t default_net_type = 1,
                     CellularModuleType module_type = CellularModuleType::ML307);    
    virtual ~DualNetworkBoard() = default;
 
    // 切换网络类型
    void SwitchNetworkType();
    
    // 获取当前网络类型
    NetworkType GetNetworkType() const { return network_type_; }
    
    // 获取当前活动的板卡引用
    Board& GetCurrentBoard() const { return *current_board_; }
    
    // 重写Board接口
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override;
    virtual std::string GetBoardJson() override;
    virtual std::string GetDeviceStatusJson() override;
};

#endif // DUAL_NETWORK_BOARD_H 
