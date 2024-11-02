#include "WifiBoard.h"
#include "BoxAudioDevice.h"

#include <esp_log.h>

#define TAG "esp32s3_korvo2_v3"

class esp32s3_korvo2_v3 : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing esp32s3_korvo2_v3");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new BoxAudioDevice();
    }
};

DECLARE_BOARD(esp32s3_korvo2_v3);
