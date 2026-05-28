#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "led_controller.h"
#include "ssid_manager.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_system.h>

#define TAG "KinconyEsp32S3AsBoard"

class KinconyEsp32S3AsBoard : public WifiBoard {
private:
    Button boot_button_;
    KinconyLedController* led_controller_;

    void InitializeAudio() {
        // Initialize MAX98357A SD MODE pin (active low)
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << AUDIO_SPK_ENABLE);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(static_cast<gpio_num_t>(AUDIO_SPK_ENABLE), 1);  // Enable amplifier
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "BOOT button pressed");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                ESP_LOGI(TAG, "Entering WiFi config mode");
                EnterWifiConfigMode();
                return;
            }
            ESP_LOGI(TAG, "Toggling chat state");
            app.ToggleChatState();
        });
        
        boot_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "BOOT button long press - resetting WiFi credentials");
            // Reset WiFi credentials
            SsidManager::GetInstance().Clear();
            ESP_LOGI(TAG, "WiFi credentials reset, rebooting...");
            // Reboot to enter config mode
            esp_restart();
        });
    }

public:
    KinconyEsp32S3AsBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "KinconyEsp32S3AsBoard constructor started");
        led_controller_ = new KinconyLedController(WS2812B_BOTTOM_GPIO, WS2812B_VERTICAL_GPIO);
        // Initialize audio early so sounds work
        ESP_LOGI(TAG, "Initializing audio codec");
        GetAudioCodec();  // This initializes the audio codec
        InitializeAudio();
        InitializeButtons();
        ESP_LOGI(TAG, "KinconyEsp32S3AsBoard constructor completed");
    }

    ~KinconyEsp32S3AsBoard() {
        delete led_controller_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Led* GetLed() override {
        return led_controller_;
    }
};

DECLARE_BOARD(KinconyEsp32S3AsBoard);