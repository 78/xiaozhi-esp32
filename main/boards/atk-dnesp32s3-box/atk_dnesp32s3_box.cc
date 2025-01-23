#include "wifi_board.h"
#include "audio_codec.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/atk_st7789_80i.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "i2c_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"


#define TAG "atk_dnesp32s3_box"

class XL9555 : public I2cDevice {
public:
    XL9555(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x06, 0x1B);   /* config IO */
        WriteReg(0x07, 0xFE);   /* config IO */
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint16_t data;
        if (bit < 8) {
            data = ReadReg(0x02);   // 读取第一个输出寄存器（0~7引脚）
        } else {
            data = ReadReg(0x03);   // 读取第二个输出寄存器（8~15引脚）
            bit -= 8;               // 将bit转换为0~7的范围
        }

        // 设置指定bit的输出状态
        data = (data & ~(1 << bit)) | (level << bit);

        // 写回对应的输出寄存器
        if (bit < 8) {
            WriteReg(0x02, data);
        } else {
            WriteReg(0x03, data);
        }
    }
};

class atk_dnesp32s3_box : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t xl9555_handle_;
    Button boot_button_;
    ATK_ST7789_80_Display* display_;
    XL9555* xl9555_;
    
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = GPIO_NUM_48,
            .scl_io_num = GPIO_NUM_45,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize XL9555
        xl9555_ = new XL9555(i2c_bus_, 0x20);
    }

    void InitializeATK_ST7789_80_Display()
    {
       
        display_ = new ATK_ST7789_80_Display(DISPLAY_BACKLIGHT_PIN,DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, 
                                             DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        xl9555_->SetOutputState(5, 1);  /* 打开喇叭 */
        xl9555_->SetOutputState(7, 1);  /* 打开背光 */
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
        {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected())
            {
                ResetWifiConfiguration();
            }
        });

        boot_button_.OnPressDown([this]()
        {
            Application::GetInstance().StartListening();
        });

        boot_button_.OnPressUp([this]()
        {
            Application::GetInstance().StopListening();
        });

        auto codec = GetAudioCodec();
        GetAudioCodec()->SetOutputVolume(50);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    atk_dnesp32s3_box() : boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeATK_ST7789_80_Display(); 
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static ATK_NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }
    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(atk_dnesp32s3_box);
