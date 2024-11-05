#include "wifi_board.h"
#include "box_audio_device.h"

#include <esp_log.h>

#define TAG "EspBox3Board"

class EspBox3Board : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing EspBox3Board");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* GetAudioDevice() override {
        static BoxAudioDevice audio_device;
        return &audio_device;
    }
};

DECLARE_BOARD(EspBox3Board);
