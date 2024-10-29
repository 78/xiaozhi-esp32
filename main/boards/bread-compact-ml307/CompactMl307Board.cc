#include "Ml307Board.h"
#include "SystemReset.h"
#include <esp_log.h>

#define TAG "CompactMl307Board"

class CompactMl307Board : public Ml307Board {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactMl307Board");
        // Check if the reset button is pressed
        SystemReset::GetInstance().CheckButtons();

        Ml307Board::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new AudioDevice();
    }
};

DECLARE_BOARD(CompactMl307Board);
