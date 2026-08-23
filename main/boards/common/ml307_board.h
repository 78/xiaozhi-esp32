#ifndef ML307_BOARD_H
#define ML307_BOARD_H

#include <memory>
#include <atomic>
#include <at_modem.h>
#include "board.h"


class Ml307Board : public Board {
protected:
    std::unique_ptr<AtModem> modem_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    gpio_num_t dtr_pin_;
    NetworkEventCallback network_event_callback_;
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> task_running_{false};
    std::atomic<int> cached_csq_{-1};
    TaskHandle_t network_task_handle_ = nullptr;
    EventGroupHandle_t lifecycle_events_ = nullptr;

    virtual std::string GetBoardJson() override;

    // Internal helper to trigger network event callback
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "");
    
    // Network initialization task (runs in FreeRTOS task)
    static void NetworkTaskEntry(void* arg);
    void NetworkTask();

public:
    Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin = GPIO_NUM_NC);
    virtual ~Ml307Board();
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual bool StopNetwork() override;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;

    bool IsNetworkRunning() const { return task_running_.load(); }
    int GetSignalQuality() const { return cached_csq_.load(); }
};

#endif // ML307_BOARD_H
