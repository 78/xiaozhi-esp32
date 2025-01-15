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
// #include "driver/adc.h"
// #include "esp_adc_cal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

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
    // SystemReset system_reset_;
    Rm67162Display *display_;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    // esp_adc_cal_characteristics_t adc_chars;

    void InitializeDisplayI2c()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = IIC_SDA_NUM,
            .scl_io_num = IIC_SCL_NUM,
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
        // thing_manager.AddThing(iot::CreateThing("Lamp"));
    }
    void InitializeAdc()
    {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc_handle));

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };

        // 创建并初始化校准句柄
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));

        // adc1_config_width(ADC_WIDTH_BIT_12);
        // adc1_config_channel_atten(BAT_DETECT_CH, ADC_ATTEN_DB_12);
        // esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
    }

public:
    LilyGoAmoled() : boot_button_(BOOT_BUTTON_GPIO),
                     touch_button_(TOUCH_BUTTON_GPIO),
                     volume_encoder_(VOLUME_ENCODER1_GPIO, VOLUME_ENCODER2_GPIO)
    // ,
    // system_reset_(RESET_NVS_BUTTON_GPIO, RESET_FACTORY_BUTTON_GPIO)
    {
        // Check if the reset button is pressed
        // system_reset_.CheckButtons();
        InitializeAdc();
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

#define VCHARGE 4100
#define V1 4000
#define V2 3800
#define V3 3600
#define V4 3400

    virtual bool GetBatteryLevel(int &level, bool &charging) override
    {
        static int last_level = 0;
        static bool last_charging = false;
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_value));
        int v1 = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_value, &v1));
        v1 *= 2;
        // ESP_LOGI(TAG, "adc_value: %d, v1: %d", adc_value, v1);
        if (v1 >= VCHARGE)
        {
            charging = true;
        }
        else if (v1 >= V1)
        {
            level = 100;
            charging = false;
        }
        else if (v1 >= V2)
        {
            level = 75;
            charging = false;
        }
        else if (v1 >= V3)
        {
            level = 50;
            charging = false;
        }
        else if (v1 >= V4)
        {
            level = 25;
            charging = false;
        }
        else
        {
            level = 0;
            charging = false;
        }

        if (level != last_level || charging != last_charging)
        {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        return true;
    }
};

DECLARE_BOARD(LilyGoAmoled);
