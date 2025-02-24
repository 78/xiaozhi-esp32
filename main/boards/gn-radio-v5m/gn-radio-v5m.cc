#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "NAU88C22.h"
#include <esp_log.h>
#include <driver/gpio.h>

#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "GN-RADIO-V5M"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_20_4);

bool first_run = true;

class NAU88C22 : public I2cDevice
{
public:
    NAU88C22(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
    {
        gpio_num_t gpio_num_4 = GPIO_NUM_4;
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_4),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_num_t gpio_num_2 = GPIO_NUM_2;
        gpio_config_t config2 = {
            .pin_bit_mask = (1ULL << gpio_num_2),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_num_t gpio_num_5 = GPIO_NUM_5;
        gpio_config_t config3 = {
            .pin_bit_mask = (1ULL << gpio_num_5),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_num_t gpio_num_12 = GPIO_NUM_12;
        gpio_config_t config4 = {
            .pin_bit_mask = (1ULL << gpio_num_12),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_num_t gpio_num_14 = GPIO_NUM_14;
        gpio_config_t config5 = {
            .pin_bit_mask = (1ULL << gpio_num_14),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

        ESP_ERROR_CHECK(gpio_config(&config));
        ESP_ERROR_CHECK(gpio_config(&config2));
        ESP_ERROR_CHECK(gpio_config(&config3));
        ESP_ERROR_CHECK(gpio_config(&config4));
        ESP_ERROR_CHECK(gpio_config(&config5));

        gpio_set_level(gpio_num_2, 1);
        gpio_set_level(gpio_num_4, 0);
        gpio_set_level(gpio_num_5, 0);
        gpio_set_level(gpio_num_12, 0);
        gpio_set_level(gpio_num_14, 0);

        if (i2c_master_probe(i2c_bus, AUDIO_CODEC_NAU88C22_ADDR, 100) == ESP_OK)
        {
            nau8822_init();
        }
    }

    void nau8822_init()
    {
        nau8822_register_write(0, 0);
        uint8_t vola = 63;
        uint8_t volb = 63;
        nau8822_register_write(OUTPUT_CONTROL, (0 << AUX1BST) | (0 << AUX2BST) | (0 << SPKBST) | (1 << TSEN) | (1 << AOUTIMP));

        nau8822_register_write(POWER_MANAGMENT_1, 0x010D);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        //		nau8822_register_write(POWER_MANAGMENT_1, 0x0E0);
        nau8822_register_write(POWER_MANAGMENT_1, (1 << PLLEN) | (1 << AUX1MXEN) | (1 << AUX2MXEN) | (1 << ABIASEN) | (1 << IOBUFEN) | REFIMP_300k);
        // nau8822_register_write(POWER_MANAGMENT_2, 0x0183);
        nau8822_register_write(POWER_MANAGMENT_2, (1 << RHPEN) | (1 << LHPEN) | (0 << SLEEP) | (1 << RBSTEN) | (1 << LBSTEN) | (1 << RPGAEN) | (1 << LPGAEN) | (1 << RADCEN) | (1 << LADCEN));
        // nau8822_register_write(POWER_MANAGMENT_3, 0x018F);
        // nau8822_register_write(POWER_MANAGMENT_3, 0x18F);
        nau8822_register_write(POWER_MANAGMENT_3, 0x18F);

        nau8822_register_write(LHP_VOLUME, (1 << LHPVU) | (0 << LHPZC) | (vola << LHPGAIN));
        nau8822_register_write(RHP_VOLUME, (1 << RHPVU) | (0 << RHPZC) | (volb << RHPGAIN));
        nau8822_register_write(LEFT_DAC_VOLUME, (1 << LDACVU) | (0xFF << LDACGAIN));
        nau8822_register_write(RIGHT_DAC_VOLUME, (1 << RDACVU) | (0xFF << RDACGAIN));
        nau8822_register_write(JACK_DETECT_1, (2 << JCKMIDEN) | (1 << JCKDIO) | (1 << JACDEN)); // 修复耳机检测功能 JCKMIDEN
        nau8822_register_write(JACK_DETECT_2, (0x0f << JCKDOEN1) | (0xf << JCKDOEN0));

        nau8822_register_write(COMPANDING, (0 << ADDAP));
        nau8822_register_write(CLOCK_CONTROL_1, (1 << CLKM) | (MCK_DIV_2 << MCLKSEL) | (BCLK_DIV_2 << BCLKSEL) | (0 << CLKIOEN));
        nau8822_register_write(CLOCK_CONTROL_2, (FILTER_SAMPLE_RATE_48KHZ << SMPLR) | (1 << SCLKEN));
        nau8822_register_write(AUDIO_INTERFACE, (WLEN_32 << WLEN) | (I2S_STANDARD << AIFMT));

        nau8822_register_write(DAC_CONTROL, (0 << DACOS) | (1 << AUTOMT));

        nau8822_register_write(ADC_CONTROL, 0);
        nau8822_register_write(EQ_1_LOW_CUTOFF, 0x002C);
        nau8822_register_write(POWER_MANAGMENT_1, 0x01FD);
        nau8822_register_write(AUX1MIXER, (0 << RMIXAUX1) | (1 << RDACAUX1));
        nau8822_register_write(AUX2MIXER, (0 << LMIXAUX2) | (1 << LDACAUX2));

        nau8822_register_write(RIGHT_SPEAKER_SUBMIXER, 0x0020);
        nau8822_register_write(36, 0x007);
        nau8822_register_write(37, 0x021); // im wiêksza wartoœæ tym szybciej odtwarza
        nau8822_register_write(38, 0x15f);
        nau8822_register_write(39, 0x126);
        nau8822_register_write(INPUT_CONTROL, (1 << LMICPLPGA) | (1 << LMICNLPGA));
        nau8822_register_write(LEFT_INPUT_PGA_GAIN, 0x13F);
        nau8822_register_write(RIGHT_INPUT_PGA_GAIN, 0x13F);
        nau8822_register_write(LEFT_ADC_BOOST, 0x107);
        nau8822_register_write(RIGHT_ADC_BOOST, 0x107);
        //   nau8822_register_write(MISC_CONTROLS, (1 << PLLLOCKBP));
        nau8822_register_write(LEFT_MIXER, (7 << LAUXMXGAIN) | (0 << LAUXLMX) | (1 << LDACLMX) | (0 << LBYPLMX));
        nau8822_register_write(RIGHT_MIXER, (7 << RAUXMXGAIN) | (0 << RAUXRMX) | (1 << RDACRMX) | (0 << RBYPRMX));
        nau8822_register_write(EQ_1_LOW_CUTOFF, (10) | (10 << EQ1CF));
        nau8822_register_write(EQ_2_PEAK_1, (10) | (10 << EQ2CF));
        nau8822_register_write(EQ_3_PEAK_2, (10) | (10 << EQ3CF));
        nau8822_register_write(EQ_4_PEAK_3, (10) | (10 << EQ4CF));
        nau8822_register_write(EQ5_HIGH_CUTOFF, (10) | (10 << EQ5CF));
        nau8822_register_write(STATUS_READOUT, (1 << AMUTCTRL));
        nau8822_register_write(NAU_GPIO, GPIO1_SET_OUTPUT_HIGH << GPIO1SEL);
    }

    void nau8822_register_write(uint8_t reg, uint16_t data)
    {
        uint8_t data_tx = data & 0xFF;
        uint8_t b8 = (data & 0x0100) >> 8;
        uint8_t cd = (uint8_t)((reg << 1) | b8);
        WriteReg(cd, data_tx);
    }
    uint16_t nau8822_register_read(uint8_t reg)
    {
        return ReadReg(reg << 1);
    }
};

class GNRadioV5MBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button power_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay *display_;
    NAU88C22 *nau88c22_;

    void InitializeI2c()
    {
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
        // Initialize NAU88C22
        nau88c22_ = new NAU88C22(i2c_bus_, AUDIO_CODEC_NAU88C22_ADDR);
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_19;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_18;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            } });
        boot_button_.OnPressDown([this]()
                                 { Application::GetInstance().StartListening(); });
        boot_button_.OnPressUp([this]()
                               { Application::GetInstance().StopListening(); });
        volume_up_button_.OnClick([this]()
                                  {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 5;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });

        volume_up_button_.OnLongPress([this]()
                                      {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 1;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });

        volume_down_button_.OnClick([this]()
                                    {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 5;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });

        volume_down_button_.OnLongPress([this]()
                                        {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 1;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });

        power_button_.OnLongPress([this]()
                                  {
                                      GetDisplay()->ShowNotification("关机");
                                      gpio_num_t gpio_num_2 = GPIO_NUM_2;
                                      gpio_config_t config2 = {
                                          .pin_bit_mask = (1ULL << gpio_num_2),
                                          .mode = GPIO_MODE_OUTPUT,
                                          .pull_up_en = GPIO_PULLUP_DISABLE,
                                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                          .intr_type = GPIO_INTR_DISABLE,
                                      };

                                      ESP_ERROR_CHECK(gpio_config(&config2));
                                      gpio_set_level(gpio_num_2, 0);
                                      esp_restart(); });
    }

    void InitializeSt7789Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_22;
        io_config.dc_gpio_num = GPIO_NUM_23;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 30 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new LcdDisplay(panel_io, panel, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_14_1,
                                      .icon_font = &font_awesome_20_4,
                                      .emoji_font = font_emoji_64_init(),
                                  });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
    }

public:
    GNRadioV5MBoard() : boot_button_(BOOT_BUTTON_GPIO), power_button_(POWER_BUTTON_GPIO), volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        if (first_run)
        {
            audio_codec.SetOutputVolume(AUDIO_DEFAULT_OUTPUT_VOLUME);
            first_run = false;
        }

        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(GNRadioV5MBoard);
