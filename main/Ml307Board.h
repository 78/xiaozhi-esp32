#ifndef ML307_BOARD_H
#define ML307_BOARD_H

#include "Board.h"
#include <Ml307AtModem.h>

class Ml307Board : public Board {
protected:
    Ml307AtModem modem_;

    void StartModem();
    void StartNetwork();

public:
    Ml307Board();
    virtual void Initialize() override;
    virtual AudioDevice* CreateAudioDevice() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual bool GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) override;
    virtual std::string GetJson() override;
};

#endif // ML307_BOARD_H
