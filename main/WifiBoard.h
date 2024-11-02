#ifndef WIFI_BOARD_H
#define WIFI_BOARD_H

#include "Board.h"

class WifiBoard : public Board {
protected:
    bool wifi_config_mode_ = false;

    virtual std::string GetBoardJson() override;

public:
    virtual void Initialize() override;
    virtual void StartNetwork() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual bool GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) override;
};

#endif // WIFI_BOARD_H
