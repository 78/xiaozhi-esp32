#include "Ml307Board.h"
#include <esp_log.h>

#define TAG "CompactMl307Board"

class CompactMl307Board : public Ml307Board {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactMl307Board");
        Ml307Board::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new AudioDevice();
    }
};

DECLARE_BOARD(CompactMl307Board);
