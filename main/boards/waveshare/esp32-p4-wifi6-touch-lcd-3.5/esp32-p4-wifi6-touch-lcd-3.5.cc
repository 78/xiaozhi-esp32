#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include "button.h"
#include "esp_video.h"
#include "esp_video_init.h"
#include "esp_lcd_panel_ops.h"
#include <driver/spi_master.h> 
#include "esp_lcd_st7796.h"
#include "config.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
#include "esp_lcd_touch_ft5x06.h"

#define TAG "WaveshareEsp32p4"

class WaveshareEsp32p4 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay *display_;
    EspVideo* camera_ = nullptr;


    esp_err_t i2c_device_probe(uint8_t addr) {
        return i2c_master_probe(i2c_bus_, addr, 100);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeLCD() {
        esp_lcd_panel_io_handle_t io = NULL;
        esp_lcd_panel_handle_t disp_panel = NULL;
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = LCD_SPI_CLK_PIN;
        buscfg.mosi_io_num = LCD_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = (DISPLAY_WIDTH * DISPLAY_HEIGHT);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.dc_gpio_num = LCD_SPI_DC_PIN;
        io_config.cs_gpio_num = LCD_SPI_CS_PIN;
        io_config.pclk_hz = (80 * 1000 * 1000);
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.spi_mode = 3;
        io_config.trans_queue_depth = 10;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));
        const esp_lcd_panel_dev_config_t lcd_dev_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .color_space = ESP_LCD_COLOR_SPACE_BGR,
            .bits_per_pixel = 16,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io, &lcd_dev_config, &disp_panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(disp_panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(disp_panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(disp_panel, true));
        esp_lcd_panel_disp_on_off(disp_panel, true);
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(disp_panel, true, false));
        display_ = new SpiLcdDisplay(io, disp_panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    void InitializeTouch()
    {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = TOUCH_RST_PIN,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
        lvgl_port_touch_cfg_t touch_cfg = {};
        touch_cfg.disp = lv_display_get_default();
        touch_cfg.handle = tp;
        lvgl_port_add_touch(&touch_cfg);
    }
    void InitializeCamera() {
        esp_video_init_csi_config_t base_csi_config = {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = i2c_bus_,
                .freq = 400000,
            },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin  = GPIO_NUM_NC,
        };

        esp_video_init_config_t cam_config = {
            .csi = &base_csi_config,
        };

        camera_ = new EspVideo(cam_config);
    }
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();
            if (state == kDeviceStateStarting ||
                state == kDeviceStateConnecting ||
                state == kDeviceStateWifiConfiguring) {
                EnterWifiConfigMode();
                return;
            }
            esp_restart();
        });
        Application::GetInstance().StartListening();
    }

public:
    WaveshareEsp32p4() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeLCD();
        InitializeTouch();
        InitializeCamera();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            true,
            false);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

};

DECLARE_BOARD(WaveshareEsp32p4);
