#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "pin_config.h"

#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "esp_lcd_gc9503.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io_additions.h>

#include "audio_codecs/box_audio_codec.h"
#include "esp_io_expander_tca9554.h"

#define TAG "ESP_S3_LCD_EV_Board"

LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

class ESP_S3_LCD_EV_Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    //add support ev board lcd
    esp_io_expander_handle_t expander = NULL;

    void InitializeRGB_GC9503V_Display() {
        ESP_LOGI(TAG, "Init GC9503V");

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        
         //add support ev board lcd
        gpio_config_t io_conf = {
            .pin_bit_mask = BIT64(GC9503V_PIN_NUM_VSYNC),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };

        gpio_config(&io_conf);
        gpio_set_level(GC9503V_PIN_NUM_VSYNC, 1);

        ESP_LOGI(TAG, "Install 3-wire SPI  panel IO");
        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_EXPANDER,
            .cs_expander_pin = GC9503V_LCD_IO_SPI_CS_1,
            .scl_io_type = IO_TYPE_EXPANDER,
            .scl_expander_pin = GC9503V_LCD_IO_SPI_SCL_1,
            .sda_io_type = IO_TYPE_EXPANDER,
            .sda_expander_pin = GC9503V_LCD_IO_SPI_SDO_1,
            .io_expander = expander,
        };

        esp_lcd_panel_io_3wire_spi_config_t io_config = GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        int espok = esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io);
        ESP_LOGI(TAG, "Install 3-wire SPI  panel IO:%d",espok);


        ESP_LOGI(TAG, "Install RGB LCD panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_PLL160M,
            //.timings = GC9503_376_960_PANEL_60HZ_RGB_TIMING(),
            //add support ev board
            .timings = GC9503_480_480_PANEL_60HZ_RGB_TIMING(),
            .data_width = 16, // RGB565 in parallel mode, thus 16bit in width
            .bits_per_pixel = 16,
            .num_fbs = GC9503V_LCD_RGB_BUFFER_NUMS,
            .bounce_buffer_size_px = GC9503V_LCD_H_RES * GC9503V_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
            .dma_burst_size = 64,
            .hsync_gpio_num = GC9503V_PIN_NUM_HSYNC,
            .vsync_gpio_num = GC9503V_PIN_NUM_VSYNC,
            .de_gpio_num = GC9503V_PIN_NUM_DE,
            .pclk_gpio_num = GC9503V_PIN_NUM_PCLK,
            .disp_gpio_num = GC9503V_PIN_NUM_DISP_EN,
            .data_gpio_nums = {
                GC9503V_PIN_NUM_DATA0,
                GC9503V_PIN_NUM_DATA1,
                GC9503V_PIN_NUM_DATA2,
                GC9503V_PIN_NUM_DATA3,
                GC9503V_PIN_NUM_DATA4,
                GC9503V_PIN_NUM_DATA5,
                GC9503V_PIN_NUM_DATA6,
                GC9503V_PIN_NUM_DATA7,
                GC9503V_PIN_NUM_DATA8,
                GC9503V_PIN_NUM_DATA9,
                GC9503V_PIN_NUM_DATA10,
                GC9503V_PIN_NUM_DATA11,
                GC9503V_PIN_NUM_DATA12,
                GC9503V_PIN_NUM_DATA13,
                GC9503V_PIN_NUM_DATA14,
                GC9503V_PIN_NUM_DATA15,
            },
            .flags= {
                .fb_in_psram = true, // allocate frame buffer in PSRAM
            }
        };
    
        ESP_LOGI(TAG, "Initialize RGB LCD panel");
    
        gc9503_vendor_config_t vendor_config = {
            .rgb_config = &rgb_config,
            .flags = {
                .mirror_by_cmd = 0,
                .auto_del_panel_io = 1,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            // .bits_per_pixel = 16,
            //add surpport ev board
            .bits_per_pixel = 18,
            .vendor_config = &vendor_config,
        };
        (esp_lcd_new_panel_gc9503(panel_io, &panel_config, &panel_handle));
        (esp_lcd_panel_reset(panel_handle));
        (esp_lcd_panel_init(panel_handle));

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_30_4,
                                      .icon_font = &font_awesome_30_4,
                                      .emoji_font = font_emoji_64_init(),
                                  });
    }
    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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

         //add support ev board lcd amp
        //初始化扩展io口
        esp_io_expander_new_i2c_tca9554(codec_i2c_bus_, 0x20, &expander);
        /* Setup power amplifier pin, set default to enable */
        esp_io_expander_set_dir(expander, BSP_POWER_AMP_IO, IO_EXPANDER_OUTPUT);
        esp_io_expander_set_level(expander, BSP_POWER_AMP_IO, true);

    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    ESP_S3_LCD_EV_Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        InitializeIot();
        InitializeRGB_GC9503V_Display();
    }


    //es7210用作音频采集
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            codec_i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            true);
        return &audio_codec;
    }



    virtual Display* GetDisplay() override {
        return display_;
    }
    
    //添加彩灯显示状态，如果亮度太暗可以去更改默认亮度值 DEFAULT_BRIGHTNESS 在led的sigle_led.cc中
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }


};

DECLARE_BOARD(ESP_S3_LCD_EV_Board);
