#include "wifi_board.h"
#include "codecs/dummy_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include <wifi_station.h>
#include <esp_log.h>

#define TAG "ESP32P4FuncEV"

class ESP32P4FunctionEvBoard : public WifiBoard {
private:
    Button boot_button_;
    Display* display_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

public:
    ESP32P4FunctionEvBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        display_ = new NoDisplay();
        InitializeButtons();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static DummyAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return nullptr;
    }
};

DECLARE_BOARD(ESP32P4FunctionEvBoard);
