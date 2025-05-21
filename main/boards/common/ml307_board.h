#ifndef ML307_BOARD_H
#define ML307_BOARD_H

#include "board.h"
#include <ml307_at_modem.h>

class Ml307Board : public Board {
protected:
    Ml307AtModem modem_;
    virtual std::string GetBoardJson() override;
    void WaitForNetworkReady();

public:
    Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, size_t rx_buffer_size = 4096);
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual Mqtt* CreateMqtt() override;
    virtual Udp* CreateUdp() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
};

#endif // ML307_BOARD_H
