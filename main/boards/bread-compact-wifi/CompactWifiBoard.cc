#include "WifiBoard.h"

#include <esp_log.h>

#define TAG "CompactWifiBoard"

class CompactWifiBoard : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactWifiBoard");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new AudioDevice();
    }
};

DECLARE_BOARD(CompactWifiBoard);
