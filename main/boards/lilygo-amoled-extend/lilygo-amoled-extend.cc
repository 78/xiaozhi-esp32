#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "esp_lcd_sh8601.c"

#include "display/rm67162_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "encoder.h"

#include "led.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "LilyGoAmoled"

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {

    {0x11, (uint8_t[]){0x00}, 0, 120},
    // {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
    // {0x35, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, // 16bits-RGB565
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

class LilyGoAmoled : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button touch_button_;
    Encoder volume_encoder_;
    SystemReset system_reset_;
    Rm67162Display *display_;

    void InitializeDisplayI2c()
    {
        // i2c_master_bus_config_t bus_config = {
        //     .i2c_port = (i2c_port_t)0,
        //     .sda_io_num = DISPLAY_SDA_PIN,
        //     .scl_io_num = DISPLAY_SCL_PIN,
        //     .clk_source = I2C_CLK_SRC_DEFAULT,
        //     .glitch_ignore_cnt = 7,
        //     .intr_priority = 0,
        //     .trans_queue_depth = 0,
        //     .flags = {
        //         .enable_internal_pullup = 1,
        //     },
        // };
        // ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetChatState() == kChatStateUnknown && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });
    }
    void InitializeEncoder()
    {
        volume_encoder_.OnPcntReach([this](int value)
                                    {
            static int lastvalue = 0;
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume();
            if(value>lastvalue)
            {
                volume += 4;
                if (volume > 100) {
                    volume = 100;
                }
            }
            else if(value<lastvalue)
            {
                volume -= 4;
                if (volume < 0) {
                    volume = 0;
                }
            }
            lastvalue = value;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });
    }

    void InitializeSpi()
    {
        ESP_LOGI(TAG, "Enable amoled power");
        gpio_set_direction(PIN_NUM_LCD_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_NUM_LCD_POWER, 1);
        ESP_LOGI(TAG, "Initialize SPI bus");
        const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(PIN_NUM_LCD_PCLK,
                                                                     PIN_NUM_LCD_DATA0,
                                                                     PIN_NUM_LCD_DATA1,
                                                                     PIN_NUM_LCD_DATA2,
                                                                     PIN_NUM_LCD_DATA3,
                                                                     DISPLAY_WIDTH * DISPLAY_HEIGHT * LCD_BIT_PER_PIXEL / 8);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
    }

    void InitializeRm67162Display()
    {
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_handle_t panel_handle = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");

        const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(PIN_NUM_LCD_CS,
                                                                                    NULL,
                                                                                    NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
        sh8601_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = LCD_BIT_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        ESP_LOGI(TAG, "Install SH8601 panel driver");
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new Rm67162Display(io_handle, panel_handle,
                                      DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    LilyGoAmoled() : boot_button_(BOOT_BUTTON_GPIO),
                     touch_button_(TOUCH_BUTTON_GPIO),
                     volume_encoder_(VOLUME_ENCODER1_GPIO, VOLUME_ENCODER2_GPIO),
                     system_reset_(RESET_NVS_BUTTON_GPIO, RESET_FACTORY_BUTTON_GPIO)
    {
        // Check if the reset button is pressed
        system_reset_.CheckButtons();

        InitializeDisplayI2c();
        InitializeSpi();
        InitializeRm67162Display();
        InitializeButtons();
        InitializeEncoder();
        InitializeIot();
    }

    virtual Led *GetBuiltinLed() override
    {
        static Led led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        ***static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                           AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                        AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(LilyGoAmoled);
