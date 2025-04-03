#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "led/circular_strip.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_nv303b.h>


#define TAG "magai_wifi"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class NV303bDisplay : public SpiLcdDisplay {
public:
    NV303bDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, 
                    {
                        .text_font = &font_puhui_16_4,
                        .icon_font = &font_awesome_16_4,
                        .emoji_font = font_emoji_32_init(),
                    }) {
    }
};

class magai_wifi : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_i80_bus_handle_t display_i80_bus_;
    NV303bDisplay* display_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;


    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
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

    void InitializeButtons() {
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }
    void InitializeNv303bDisplay() {
        esp_lcd_panel_io_handle_t pannel_io = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_i80_bus_config_t bus_config = {};
        bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
        bus_config.dc_gpio_num = DISPLAY_PIN_NUM_DC;
        bus_config.wr_gpio_num = DISPLAY_PIN_NUM_PCLK;
        bus_config.data_gpio_nums[0] = DISPLAY_PIN_NUM_DATA0;
        bus_config.data_gpio_nums[1] = DISPLAY_PIN_NUM_DATA1;
        bus_config.data_gpio_nums[2] = DISPLAY_PIN_NUM_DATA2;
        bus_config.data_gpio_nums[3] = DISPLAY_PIN_NUM_DATA3;
        bus_config.data_gpio_nums[4] = DISPLAY_PIN_NUM_DATA4;
        bus_config.data_gpio_nums[5] = DISPLAY_PIN_NUM_DATA5;
        bus_config.data_gpio_nums[6] = DISPLAY_PIN_NUM_DATA6;
        bus_config.data_gpio_nums[7] = DISPLAY_PIN_NUM_DATA7;
        bus_config.bus_width = 8;
        bus_config.max_transfer_bytes = DISPLAY_WIDTH * 100 * sizeof(uint16_t);
        bus_config.dma_burst_size = DISPLAY_DMA_BURST_SIZE;
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &display_i80_bus_));

        esp_lcd_panel_io_i80_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_PIN_NUM_CS;
        io_config.pclk_hz = DISPLAY_LCD_PIXEL_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.dc_levels.dc_idle_level  = 0;
        io_config.dc_levels.dc_cmd_level   = 0;
        io_config.dc_levels.dc_dummy_level = 0;
        io_config.dc_levels.dc_data_level  = 1;
        io_config.flags.swap_color_bytes = 0;
        io_config.lcd_cmd_bits = DISPLAY_LCD_CMD_BITS;
        io_config.lcd_param_bits = DISPLAY_LCD_PARAM_BITS;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(display_i80_bus_, &io_config, &pannel_io));

        ESP_LOGI(TAG, "Install LCD driver of st7789");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_PIN_NUM_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv303b(pannel_io, &panel_config, &panel_handle));

        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);

        esp_lcd_panel_invert_color(panel_handle, true);
        esp_lcd_panel_swap_xy(panel_handle, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_set_gap(panel_handle, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        display_ = new NV303bDisplay(pannel_io, panel_handle,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

    }
    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }

public:
    magai_wifi() :
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeI2c();
        InitializeButtons();
        InitializeNv303bDisplay();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, BUILTIN_LED_NUM);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(magai_wifi);
