#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>

#include "power_manager.h"

#define TAG "FogSeekAus3V1"

class FogSeekAus3V1 : public WifiBoard {
private:
    Button boot_button_;
    Button pwr_button_;
    PowerManager* power_manager_;
    bool power_save_mode_ = false;
    void InitializePowerManager() {
            power_manager_ = new PowerManager(PWR_CHARGEINGE_GPIO);
            power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                gpio_set_level(LED_1_GPIO, 1);
                gpio_set_level(LED_2_GPIO, 0);
            } else {
                gpio_set_level(LED_1_GPIO, 0);
                gpio_set_level(LED_2_GPIO, 1);
            }        
        });
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
        pwr_button_.OnLongPress([this]() {
            if(power_save_mode_ == false){
                //初始化电源控制引脚
                gpio_config_t io_conf = {};
                io_conf.intr_type = GPIO_INTR_DISABLE;
                io_conf.mode = GPIO_MODE_OUTPUT;
                io_conf.pin_bit_mask = (1ULL << PWR_CTRL_GPIO);
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
                io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
                gpio_config(&io_conf);
                gpio_set_level(PWR_CTRL_GPIO, 1); // 设置电源控制引脚为高电平
                ESP_LOGI(TAG, "Power control pin set to HIGH for keeping power.");  
                power_save_mode_ = true; 
                io_conf.pin_bit_mask = (1ULL << LED_2_GPIO);
                gpio_config(&io_conf);
                io_conf.pin_bit_mask = (1ULL << LED_1_GPIO);
                gpio_config(&io_conf);
                gpio_set_level(LED_2_GPIO, 1); // 设置电源控制引脚为高电平
            }else{
                gpio_set_level(LED_2_GPIO, 0);
                gpio_set_level(LED_1_GPIO, 0);                
                gpio_set_level(PWR_CTRL_GPIO, 0); // 设置电源控制引脚为低电平  
                ESP_LOGI(TAG, "Power control pin set to Low for shutdown.");  
            }
        });
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeIot() {
#if CONFIG_IOT_PROTOCOL_XIAOZHI
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
#elif CONFIG_IOT_PROTOCOL_MCP
        static LampController lamp(GPIO_NUM_16);
#endif
    }

public:
    FogSeekAus3V1() :
        boot_button_(BOOT_BUTTON_GPIO), pwr_button_(PWR_BUTTON_GPIO) {
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }
};

DECLARE_BOARD(FogSeekAus3V1);
