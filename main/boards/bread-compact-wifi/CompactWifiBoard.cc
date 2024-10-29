#include "WifiBoard.h"
#include "SystemReset.h"
#include <esp_log.h>

#define TAG "CompactWifiBoard"

class CompactWifiBoard : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactWifiBoard");
        // Check if the reset button is pressed
        SystemReset::GetInstance().CheckButtons();

        WifiBoard::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new AudioDevice();
    }
};

DECLARE_BOARD(CompactWifiBoard);
