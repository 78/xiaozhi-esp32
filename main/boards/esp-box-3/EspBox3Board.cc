#include "WifiBoard.h"
#include "BoxAudioDevice.h"

#include <esp_log.h>

#define TAG "EspBox3Board"

class EspBox3Board : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing EspBox3Board");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new BoxAudioDevice();
    }
};

DECLARE_BOARD(EspBox3Board);
