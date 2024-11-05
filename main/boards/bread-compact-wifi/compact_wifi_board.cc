#include "wifi_board.h"
#include "system_reset.h"
#include "audio_device.h"

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

    virtual AudioDevice* GetAudioDevice() override {
        static AudioDevice audio_device;
        return &audio_device;
    }
};

DECLARE_BOARD(CompactWifiBoard);
