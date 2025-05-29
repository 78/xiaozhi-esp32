#include "wifi_board.h"
#include "adc_pdm_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#include "display/lcd_display.h"
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_lcd_ili9341.h"

#include "assets/lang_config.h"
#include "anim_player.h"
#include "emoji_display.h"
#include "servo_dog_ctrl.h"

#define TAG "ESP_HI"

static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, NULL, 0, 120},     // Sleep out, Delay 120ms
    {0xB1, (uint8_t []){0x05, 0x3A, 0x3A}, 3, 0},
    {0xB2, (uint8_t []){0x05, 0x3A, 0x3A}, 3, 0},
    {0xB3, (uint8_t []){0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A}, 6, 0},
    {0xB4, (uint8_t []){0x03}, 1, 0},   // Dot inversion
    {0xC0, (uint8_t []){0x44, 0x04, 0x04}, 3, 0},
    {0xC1, (uint8_t []){0xC0}, 1, 0},
    {0xC2, (uint8_t []){0x0D, 0x00}, 2, 0},
    {0xC3, (uint8_t []){0x8D, 0x6A}, 2, 0},
    {0xC4, (uint8_t []){0x8D, 0xEE}, 2, 0},
    {0xC5, (uint8_t []){0x08}, 1, 0},
    {0xE0, (uint8_t []){0x0F, 0x10, 0x03, 0x03, 0x07, 0x02, 0x00, 0x02, 0x07, 0x0C, 0x13, 0x38, 0x0A, 0x0E, 0x03, 0x10}, 16, 0},
    {0xE1, (uint8_t []){0x10, 0x0B, 0x04, 0x04, 0x10, 0x03, 0x00, 0x03, 0x03, 0x09, 0x17, 0x33, 0x0B, 0x0C, 0x06, 0x10}, 16, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x05}, 1, 0},
    {0x36, (uint8_t []){0xC8}, 1, 0},
    {0x29, NULL, 0, 0},     // Display on
    {0x2C, NULL, 0, 0},     // Memory write
};

class EspHi : public WifiBoard {
private:
    Button boot_button_;
    Button audio_wake_button_;
    Button move_wake_button_;
    anim::EmojiWidget* display_ = nullptr;

    void HandleMoveWakePressDown(int64_t current_time, int64_t &last_trigger_time, int &gesture_state)
    {
        int64_t interval = last_trigger_time == 0 ? 0 : current_time - last_trigger_time;
        last_trigger_time = current_time;

        if (interval > 1000) {
            gesture_state = 0;
        } else {
            switch (gesture_state) {
            case 0:
                break;
            case 1:
                if (interval > 300) {
                    gesture_state = 2;
                }
                break;
            case 2:
                if (interval > 100) {
                    gesture_state = 0;
                }
                break;
            }
        }
    }

    void HandleMoveWakePressUp(int64_t current_time, int64_t &last_trigger_time, int &gesture_state)
    {
        int64_t interval = current_time - last_trigger_time;

        if (interval > 1000) {
            gesture_state = 0;
        } else {
            switch (gesture_state) {
            case 0:
                if (interval > 300) {
                    gesture_state = 1;
                }
                break;
            case 1:
                break;
            case 2:
                if (interval < 100) {
                    ESP_LOGI(TAG, "gesture detected");
                    gesture_state = 0;
                    auto &app = Application::GetInstance();
                    app.ToggleChatState();
                }
                break;
            }
        }
    }

    void InitializeButtons()
    {
        static int64_t last_trigger_time = 0;
        static int gesture_state = 0;  // 0: init, 1: wait second long interval, 2: wait oscillation

        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        audio_wake_button_.OnPressDown([this]() {
        });

        audio_wake_button_.OnPressUp([this]() {
        });

        move_wake_button_.OnPressDown([this]() {
            int64_t current_time = esp_timer_get_time() / 1000;
            HandleMoveWakePressDown(current_time, last_trigger_time, gesture_state);
        });

        move_wake_button_.OnPressUp([this]() {
            int64_t current_time = esp_timer_get_time() / 1000;
            HandleMoveWakePressUp(current_time, last_trigger_time, gesture_state);
        });
    }

    void InitializeIot()
    {
        ESP_LOGI(TAG, "Initialize Iot");
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("DogLight"));
        thing_manager.AddThing(iot::CreateThing("DogAction_basic"));
        thing_manager.AddThing(iot::CreateThing("DogAction_extra"));
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * 10 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const ili9341_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *) &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_set_gap(panel, 0, 24);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "LCD panel create success, %p", panel);

        esp_lcd_panel_disp_on_off(panel, true);

        ESP_LOGI(TAG, "Create emoji widget, panel: %p, panel_io: %p", panel, panel_io);
        display_ = new anim::EmojiWidget(panel, panel_io);

        servo_dog_ctrl_config_t config = {
            .fl_gpio_num = FL_GPIO_NUM,
            .fr_gpio_num = FR_GPIO_NUM,
            .bl_gpio_num = BL_GPIO_NUM,
            .br_gpio_num = BR_GPIO_NUM,
        };
#if CONFIG_ESP_CONSOLE_NONE
        servo_dog_ctrl_init(&config);
#endif
    }

public:
    EspHi() : boot_button_(BOOT_BUTTON_GPIO),
        audio_wake_button_(AUDIO_WAKE_BUTTON_GPIO),
        move_wake_button_(MOVE_WAKE_BUTTON_GPIO)
    {

        InitializeButtons();
        InitializeIot();
        InitializeSpi();
        InitializeLcdDisplay();
    }

    virtual AudioCodec* GetAudioCodec() override
    {
        static AdcPdmAudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_ADC_MIC_CHANNEL,
            AUDIO_PDM_SPEAK_P_GPIO,
            AUDIO_PDM_SPEAK_N_GPIO,
            AUDIO_PA_CTL_GPIO);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(EspHi);
