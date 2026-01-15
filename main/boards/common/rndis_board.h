#ifndef RNDIS_BOARD_H
#define RNDIS_BOARD_H

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S3
#include "board.h"
#include "iot_eth.h"
#include "iot_usbh_rndis.h"
#include "iot_eth_netif_glue.h"
#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>

class RndisBoard : public Board {
private:
    EventGroupHandle_t s_event_group = nullptr;
    iot_eth_driver_t *rndis_eth_driver = nullptr;
    esp_netif_t *s_rndis_netif = nullptr;

    void install_rndis(uint16_t idVendor, uint16_t idProduct, const char *netif_name);
    static void iot_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
protected:
    NetworkEventCallback network_event_callback_ = nullptr;

    virtual std::string GetBoardJson() override;

    /**
     * Handle network event (called from WiFi manager callbacks)
     * @param event The network event type
     * @param data Additional data (e.g., SSID for Connecting/Connected events)
     */
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "");

    /**
     * Start WiFi connection attempt
     */
    void TryWifiConnect();

    /**
     * Enter WiFi configuration mode
     */
    void StartWifiConfigMode();

    /**
     * WiFi connection timeout callback
     */
    static void OnWifiConnectTimeout(void* arg);

public:
    RndisBoard();
    virtual ~RndisBoard();
    
    virtual std::string GetBoardType() override;
    
    /**
     * Start network connection asynchronously
     * This function returns immediately. Network events are notified through the callback set by SetNetworkEventCallback().
     */
    virtual void StartNetwork() override;
    
    virtual NetworkInterface* GetNetwork() override;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
    
};
#endif // CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S3

#endif // RNDIS_BOARD_H
