#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7735.h>


#define TAG "esp32-cgc"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

static const st7735_lcd_init_cmd_t st7735_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {ST7735_SWRESET, (uint8_t[]){0x00}, 1, 150},                                                                                          // Software reset, 150 ms delay
    {ST7735_SLPOUT, (uint8_t[]){0x00}, 1, 255},                                                                                           // Out of sleep mode, 255 ms delay
    {ST7735_FRMCTR1, (uint8_t[]){0x01, 0x2C, 0x2D}, 3, 0},                                                                                // Frame rate ctrl - normal mode
    {ST7735_FRMCTR2, (uint8_t[]){0x01, 0x2C, 0x2D}, 3, 0},                                                                                // Frame rate control - idle mode
    {ST7735_FRMCTR3, (uint8_t[]){0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6, 0},                                                              // Frame rate ctrl - partial mode
    {ST7735_INVCTR, (uint8_t[]){0x07}, 1, 0},                                                                                             // Display inversion ctrl
    {ST7735_PWCTR1, (uint8_t[]){0xA2, 0x02, 0x84}, 3, 0},                                                                                 // Power control
    {ST7735_PWCTR2, (uint8_t[]){0xC5}, 1, 0},                                                                                             // Power control
    {ST7735_PWCTR3, (uint8_t[]){0x0A, 0x00}, 2, 0},                                                                                       // Power control
    {ST7735_PWCTR4, (uint8_t[]){0x8A, 0x2A}, 2, 0},                                                                                       // Power control
    {ST7735_PWCTR5, (uint8_t[]){0x8A, 0xEE}, 2, 0},                                                                                       // Power control
    {ST7735_VMCTR1, (uint8_t[]){0x0E}, 1, 0},                                                                                             // Power control
    {ST7735_INVOFF, (uint8_t[]){0x00}, 1, 0},                                                                                             // Don't invert display
    {ST7735_COLMOD, (uint8_t[]){0x05}, 1, 0},                                                                                             // Set color mode (16-bit)
    {ST7735_GMCTRP1, (uint8_t[]){0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16, 0}, // Positive Gamma
    {ST7735_GMCTRN1, (uint8_t[]){0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16, 0}, // Negative Gamma
    {ST7735_NORON, (uint8_t[]){0x00}, 1, 0},                                                                                 // Normal display on, no args, w/delay 10 ms delay
    {ST7735_DISPON, (uint8_t[]){0x00}, 1, 0},
};

class CompactWifiBoard : public WifiBoard {
private:
    Display* display_;
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7735Display() {
        ESP_LOGI(TAG, "Init ST7735 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_SPI_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
    
        ESP_LOGI(TAG, "Install ST7735 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        st7735_vendor_config_t st7735_vendor_config = {
            .init_cmds = st7735_lcd_init_cmds,
            .init_cmds_size = sizeof(st7735_lcd_init_cmds) / sizeof(st7735_lcd_init_cmd_t),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
        panel_config.bits_per_pixel = 16; // Implemented by LCD command `3Ah` (16/18)
        panel_config.vendor_config = &st7735_vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); 

        display_ = new LcdDisplay(io_handle, panel_handle, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_14_1,
                                        .icon_font = &font_awesome_14_1,
                                        .emoji_font = font_emoji_32_init(),
                                    });
    }

    void InitializeButtons() {
        
        // 配置 GPIO
        // gpio_config_t io_conf = {
        //     // .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // 设置需要配置的 GPIO 引脚
        //     .mode = GPIO_MODE_OUTPUT,           // 设置为输出模式
        //     .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
        //     .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
        //     .intr_type = GPIO_INTR_DISABLE      // 禁用中断
        // };
        // gpio_config(&io_conf);  // 应用配置

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            // gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word="你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        touch_button_.OnPressDown([this]() {
            // gpio_set_level(BUILTIN_LED_GPIO, 1);
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            // gpio_set_level(BUILTIN_LED_GPIO, 0);
            Application::GetInstance().StopListening();
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(iot::CreateThing("Newfan"));
    }

public:
    CompactWifiBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeSpi();
        InitializeSt7735Display();
        InitializeButtons();
        InitializeIot();
    }

    // virtual Led* GetLed() override {
    //     static SingleLed led(BUILTIN_LED_GPIO);
    //     return &led;
    // }


    virtual AudioCodec* GetAudioCodec() override 
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
