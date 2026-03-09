#ifndef NT26_BOARD_H
#define NT26_BOARD_H

#include <memory>
#include <uart_eth_modem.h>
#include <esp_network.h>
#include <esp_pm.h>
#include <esp_timer.h>
#include "board.h"

struct Nt26CeregState {
    int stat = 0;
    std::string tac;
    std::string ci;
    int AcT = -1;

    std::string ToString() const {
        std::string json = "{";
        json += "\"stat\":" + std::to_string(stat);
        if (!tac.empty()) json += ",\"tac\":\"" + tac + "\"";
        if (!ci.empty()) json += ",\"ci\":\"" + ci + "\"";
        if (AcT >= 0) json += ",\"AcT\":" + std::to_string(AcT);
        json += "}";
        return json;
    }
};

class Nt26Board : public Board {
protected:
    std::unique_ptr<UartEthModem> modem_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    gpio_num_t dtr_pin_; // mrdy_pin
    gpio_num_t ri_pin_;  // srdy_pin
    gpio_num_t reset_pin_;
    
    NetworkEventCallback network_event_callback_;
    esp_pm_lock_handle_t pm_lock_cpu_max_ = nullptr;
    PowerSaveLevel current_power_level_ = PowerSaveLevel::LOW_POWER;
    esp_timer_handle_t network_ready_timer_ = nullptr;

    virtual std::string GetBoardJson() override;
    
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "");
    static void OnNetworkReadyTimeout(void* arg);
    void ScheduleAsyncStop();

public:
    Nt26Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin, gpio_num_t ri_pin, gpio_num_t reset_pin = GPIO_NUM_NC);
    virtual ~Nt26Board();
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
    Nt26CeregState GetRegistrationState();
};

#endif // NT26_BOARD_H
