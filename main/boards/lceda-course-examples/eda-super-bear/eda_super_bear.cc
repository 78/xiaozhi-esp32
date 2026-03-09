#include <esp_log.h>

#include "application.h"
#include "codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wifi_board.h"

#define TAG "EdaSuperBear"

extern void InitializeEdaSuperBearController();

class EdaSuperBear : public WifiBoard {
private:
    Display* display_;
    Button boot_button_;

    void InitializeDisplay() {
        display_ = new NoDisplay();
        ESP_LOGI(TAG, "使用NoDisplay (无物理显示屏)");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    EdaSuperBear() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplay();
        InitializeButtons();
        ESP_LOGI(TAG, "初始化EdaRobot机器人MCP控制器");
        ::InitializeEdaSuperBearController();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(EdaSuperBear);
