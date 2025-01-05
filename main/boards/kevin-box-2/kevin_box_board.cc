#include "ml307_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "axp2101.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_timer.h>

#define TAG "KevinBoxBoard"

class KevinBoxBoard : public Ml307Board {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    Axp2101* axp2101_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    uint8_t _data_buffer[2];
    esp_timer_handle_t power_save_timer_ = nullptr;

    void InitializePowerSaveTimer() {
        esp_timer_create_args_t power_save_timer_args = {
            .callback = [](void *arg) {
                auto board = static_cast<KevinBoxBoard*>(arg);
                board->PowerSaveCheck();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "Power Save Timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&power_save_timer_args, &power_save_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));
    }

    void PowerSaveCheck() {
        // 电池放电模式下，如果待机超过一定时间，则自动关机
        const int seconds_to_shutdown = 600;
        static int seconds = 0;
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
            seconds = 0;
            return;
        }
        if (!axp2101_->IsDischarging()) {
            seconds = 0;
            return;
        }
        
        seconds++;
        if (seconds >= seconds_to_shutdown) {
            axp2101_->PowerOff();
        }
    }

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
        // Make GPIO HIGH to enable the 4G module
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 4),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_4, 1);
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification("最大音量");
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification("已静音");
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeCodecI2c();
        axp2101_ = new Axp2101(codec_i2c_bus_, AXP2101_I2C_ADDR);

        MountStorage();
        Enable4GModule();

        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeIot();
    }
    
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        return &display;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;
        level = axp2101_->GetBatteryLevel();
        charging = axp2101_->IsCharging();
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);