#include "wifi_board.h"
#include "k10_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#include "esp_io_expander_tca95xx_16bit.h"

#define TAG "DF-K10"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

static void pin_2_12_status_task(void *arg);

class Df_K10Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    LcdDisplay *display_;

    void InitializeI2c() {
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
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_21;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_12;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }
    void InitializeIoExpander() {
        esp_io_expander_new_i2c_tca95xx_16bit(
                i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_000, &io_expander);

        esp_err_t ret;
        ret = esp_io_expander_print_state(io_expander);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Print state failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0,
                                                                    IO_EXPANDER_OUTPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set direction failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_level(io_expander, 0, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set level failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(
                io_expander, (IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_12),
                IO_EXPANDER_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set direction failed: %s", esp_err_to_name(ret));
        }

        xTaskCreate(pin_2_12_status_task, "pin_2_12_status_task", 8*1024,
                                (void *)this, 5, NULL);
    }
    void InitializeButtons() {
    }

    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;
        io_config.dc_gpio_num = GPIO_NUM_13;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 16;
        panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    esp_io_expander_handle_t io_expander;
    Df_K10Board() {
        InitializeI2c();
        InitializeIoExpander();
        InitializeSpi();
        InitializeIli9341Display();
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static K10AudioCodec audio_codec(
                    i2c_bus_,
                    AUDIO_INPUT_SAMPLE_RATE,
                    AUDIO_OUTPUT_SAMPLE_RATE,
                    AUDIO_I2S_GPIO_MCLK,
                    AUDIO_I2S_GPIO_BCLK,
                    AUDIO_I2S_GPIO_WS,
                    AUDIO_I2S_GPIO_DOUT,
                    AUDIO_I2S_GPIO_DIN,
                    AUDIO_CODEC_PA_PIN,
                    AUDIO_CODEC_ES8311_ADDR,
                    AUDIO_CODEC_ES7210_ADDR,
                    AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }
};

static void pin_2_12_status_task(void *arg) {
    Df_K10Board *board = (Df_K10Board *)arg;    // Use pointer to the board object
    uint32_t input_level_mask = 0;
    uint32_t prev_input_level_mask = 0; // To store the previous state
    uint32_t debounce_delay = 50 / portTICK_PERIOD_MS; // Set debounce time (50 ms)

    while (1) {
        vTaskDelay(20 / portTICK_PERIOD_MS); // Regular task delay

        esp_err_t ret = esp_io_expander_get_level(
                board->io_expander, (IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_12),
                &input_level_mask);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get level mask: %s", esp_err_to_name(ret));
            continue;
        }

        // If the state has changed, start debounce process
        if (input_level_mask != prev_input_level_mask) {
            // State changed, wait for debounce time
            vTaskDelay(debounce_delay);

            // Re-check the state after debounce time
            ret = esp_io_expander_get_level(
                    board->io_expander, (IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_12),
                    &input_level_mask);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get level mask: %s", esp_err_to_name(ret));
                continue;
            }

            // If the state is still the same, consider it stable and log
            if (input_level_mask != prev_input_level_mask) {
                prev_input_level_mask = input_level_mask;

                // Now check the stable state and log the button press
                if (!(input_level_mask & IO_EXPANDER_PIN_NUM_2)) {
                    ESP_LOGI(TAG, "Button B pressed");
                    auto &app = Application::GetInstance();
                        if (app.GetDeviceState() == kDeviceStateStarting &&
                                !WifiStation::GetInstance().IsConnected()) {
                             board->ResetWifiConfiguration();

                        }
                    app.ToggleChatState();
                }

                if (!(input_level_mask & IO_EXPANDER_PIN_NUM_12)) {
                    ESP_LOGI(TAG, "Button A pressed");
                    auto &app = Application::GetInstance();
                        if (app.GetDeviceState() == kDeviceStateStarting &&
                                !WifiStation::GetInstance().IsConnected()) {
                            board->ResetWifiConfiguration();
                        }
                    app.ToggleChatState();
                }
            }
        }

        // No change in state, simply continue with the loop
    }
}



DECLARE_BOARD(Df_K10Board);