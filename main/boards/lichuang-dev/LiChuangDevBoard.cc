#include "WifiBoard.h"
#include "BoxAudioDevice.h"

#include <esp_log.h>

#define TAG "LiChuangDevBoard"

class LiChuangDevBoard : public WifiBoard {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing LiChuangDevBoard");
        WifiBoard::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new BoxAudioDevice();
    }
};

DECLARE_BOARD(LiChuangDevBoard);
