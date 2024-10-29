#ifndef WIFI_BOARD_H
#define WIFI_BOARD_H

#include "Board.h"

class WifiBoard : public Board {
protected:
    bool wifi_config_mode_ = false;
    virtual void StartWifi();

public:
    virtual void Initialize() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual bool GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) override;
    virtual std::string GetJson() override;
};

#endif // WIFI_BOARD_H
