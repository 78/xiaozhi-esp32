#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include <esp_log.h>

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "system_info.h"
#include <esp_chip_info.h>
#include "esp_system.h"

#include "board.h"
#include "system_info.h"
#include "settings.h"
#include "assets/lang_config.h"
#include "mcp_server.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_random.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "application.h"
#include "driver/rtc_io.h"
#include "led_eye.h"
#include "wifi_station.h"

#define TAG "pdi_chatbox_v1"


class ES7210_ES8311_AudioCodec : public BoxAudioCodec {
public:
    ES7210_ES8311_AudioCodec(i2c_master_bus_handle_t i2c_bus) 
        : BoxAudioCodec(i2c_bus, 
                       AUDIO_INPUT_SAMPLE_RATE, 
                       AUDIO_OUTPUT_SAMPLE_RATE,
                       AUDIO_I2S_GPIO_MCLK, 
                       AUDIO_I2S_GPIO_BCLK, 
                       AUDIO_I2S_GPIO_WS, 
                       AUDIO_I2S_GPIO_DOUT, 
                       AUDIO_I2S_GPIO_DIN,
                       AUDIO_PA_CTRL, 
                       AUDIO_CODEC_ES8311_ADDR, 
                       AUDIO_CODEC_ES7210_ADDR, 
                       AUDIO_INPUT_REFERENCE) {
    }
};

class pdi_chatbox_v1 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    uint32_t idleCount;
    esp_timer_handle_t clock_timer_handle_ = nullptr;

    static void TimerShutDown(void* arg){
        static uint32_t count;
        auto &app=Application::GetInstance();
        pdi_chatbox_v1* instance = static_cast<pdi_chatbox_v1*>(arg);
        if(app.GetDeviceState() == kDeviceStateIdle){
            count+=1;
        }else{
            count=0;
        }
        if(count >= POWER_OFF_TIMER){
            instance->ShutDown();
        }
    }
    void ShutDown(){
        esp_wifi_stop();
        esp_wifi_deinit();
        auto& app = Application::GetInstance();
        app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(KEY_HOLD_GPIO, 0);
    }
    void InitShowDownTimer() {
        esp_timer_create_args_t timer_args = {
            .callback = &pdi_chatbox_v1::TimerShutDown, // 静态函数
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "TimerShutDown",
            .skip_unhandled_events = true
        };
        esp_timer_create(&timer_args, &clock_timer_handle_);
        esp_timer_start_periodic(clock_timer_handle_, 1000000); // 1秒
    }

    void InitializeI2c() {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitPowerGpio(){
        // 配置 GPIO
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE; // 禁用中断
        io_conf.mode = GPIO_MODE_OUTPUT;        // 设置为输出模式
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;               // 禁用下拉
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;                    // 使能上拉
        io_conf.pin_bit_mask = (1ULL << KEY_HOLD_GPIO);             // 设置输出引脚
        gpio_config(&io_conf);                                      // 应用配置
        gpio_set_level(KEY_HOLD_GPIO, 1);                           //打开电源                         
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            static  uint8_t wifi_button_times=0;
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                wifi_button_times++;
            }
            if(wifi_button_times>2){
                ResetWifiConfiguration();
            }
            else{
                app.ToggleChatState();
            }
        });
        boot_button_.OnLongPress([this]() {
            ShutDown();
        });
    }

    void InitializeTools() {
        
    }
    
public:
    pdi_chatbox_v1() : boot_button_(BOOT_BUTTON_GPIO,false,5000,50) {
        InitPowerGpio();
        InitializeI2c();
        InitializeButtons();
        InitializeTools();
        InitShowDownTimer();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static ES7210_ES8311_AudioCodec audio_codec(i2c_bus_);
        return &audio_codec;
    }

    virtual Led* GetLed() override {
        static LedEye led(BUILTIN_LED_GPIO);
        return &led;
    }
};
DECLARE_BOARD(pdi_chatbox_v1);
