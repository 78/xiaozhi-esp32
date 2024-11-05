#include "wifi_board.h"
#include "box_audio_device.h"

#include <esp_log.h>

#define TAG "LichuangDevBoard"

class LichuangDevBoard : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing LichuangDevBoard");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* GetAudioDevice() override {
        static BoxAudioDevice audio_device;
        return &audio_device;
    }
};

DECLARE_BOARD(LichuangDevBoard);
