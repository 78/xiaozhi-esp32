#include "Ml307Board.h"
#include "BoxAudioDevice.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <driver/gpio.h>

static const char *TAG = "KevinBoxBoard";

class KevinBoxBoard : public Ml307Board {
private:
    void MountStorage() {
        // Mount the storage partition
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/storage",
            .partition_label = "storage",
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        esp_vfs_spiffs_register(&conf);
    }

    void Enable4GModule() {
        // Make GPIO15 HIGH to enable the 4G module
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 15),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_15, 1);
    }
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing KevinBoxBoard");
        MountStorage();
        Enable4GModule();
        Ml307Board::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new BoxAudioDevice();
    }
};

DECLARE_BOARD(KevinBoxBoard);
