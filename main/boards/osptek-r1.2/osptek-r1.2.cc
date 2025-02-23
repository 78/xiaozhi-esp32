#include "wifi_board.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"

#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include "audio_codecs/es8311_audio_codec.h"
#include <wifi_station.h>
#define TAG "kevin-sp-v3"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class OSPTEK_R1_2Board : public WifiBoard
{

private:
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    LcdDisplay *display_;
    AudioCodec *audio_codec_;
    Button boot_button_;

    void InitializeSpi()
    {
        ESP_LOGD(TAG, "Install panel IO");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = ST7789_GPIO_MOSI;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = ST7789_GPIO_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = ST7789_GPIO_CS;
        io_config.dc_gpio_num = ST7789_GPIO_DC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = ST7789_GPIO_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        // 重要：按照正确的顺序初始化显示屏
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 40, 53));

        display_ = new LcdDisplay(panel_io, panel, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
                                    });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
    }

    void InitializeCodecI2c()
    {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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

    void InitializeButtons()
    {
         boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]()
                                 { Application::GetInstance().StartListening(); });
        boot_button_.OnPressUp([this]()
                               { Application::GetInstance().StopListening(); });

        // 添加音量按钮的初始化
        Button volume_up_button_(VOLUME_UP_BUTTON_GPIO);
        volume_up_button_.OnPressDown([]()
                                      {
            // 处理音量增加的逻辑
            ESP_LOGI(TAG, "Volume Up Button Pressed"); });

        Button volume_down_button_(VOLUME_DOWN_BUTTON_GPIO);
        volume_down_button_.OnPressDown([]()
                                        {
            // 处理音量减少的逻辑
            ESP_LOGI(TAG, "Volume Down Button Pressed"); });
    }

public:
    OSPTEK_R1_2Board() : audio_codec_(nullptr),
                         boot_button_(BOOT_BUTTON_GPIO)
    {
        ESP_LOGI(TAG, "Initializing OSPTEK_R1_2 Board");

        InitializeSpi();
        InitializeCodecI2c();
        InitializeButtons();
        InitializeSt7789Display();
        InitializeIot();
    }

    ~OSPTEK_R1_2Board()
    {
        if (audio_codec_ != nullptr)
        {
            delete audio_codec_;
            audio_codec_ = nullptr;
        }
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        ESP_LOGD(TAG, "Install Audio driver");
        if (audio_codec_ == nullptr)
        {
            audio_codec_ = new Es8311AudioCodec(
                codec_i2c_bus_,           // i2c_master_handle
                I2C_NUM_1,                // i2c_port
                AUDIO_INPUT_SAMPLE_RATE,  // input_sample_rate
                AUDIO_OUTPUT_SAMPLE_RATE, // output_sample_rate
                AUDIO_I2S_GPIO_MCLK,      // mclk
                AUDIO_I2S_GPIO_BCLK,      // bclk
                AUDIO_I2S_GPIO_WS,        // ws
                AUDIO_I2S_GPIO_DOUT,      // dout
                AUDIO_I2S_GPIO_DIN,       // din
                GPIO_NUM_NC,              // pa_pin (不使用)
                AUDIO_CODEC_ES8311_ADDR   // es8311_addr
            );

            if (audio_codec_ == nullptr)
            {
                ESP_LOGE(TAG, "Failed to create ES8311 audio codec");
                return nullptr;
            }
        }
        return audio_codec_;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(OSPTEK_R1_2Board);
