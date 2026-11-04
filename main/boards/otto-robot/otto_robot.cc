
#include <driver/spi_common.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#include "application.h"
#include "codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "otto_emoji_display.h"
#include "otto_gpio_config.h"
#include "otto_head_controller.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "websocket_control_server.h"

#include <new>
#include <cstdlib>

#define TAG "OttoRobot"

class OttoRobot : public WifiBoard {
private:
    LcdDisplay* display_ = nullptr;
    OttoGpioConfig gpio_config_;
    OttoHeadController head_controller_;
    Button boot_button_;
    WebSocketControlServer* ws_control_server_ = nullptr;

    bool spi_initialized_ = false;
    bool backlight_ready_ = false;

    static bool IsOutputPin(int pin) {
        return pin >= 0 &&
               GPIO_IS_VALID_GPIO(static_cast<gpio_num_t>(pin)) &&
               GPIO_IS_VALID_OUTPUT_GPIO(static_cast<gpio_num_t>(pin));
    }

    bool IsDisplayConfigValid() const {
        const int pins[] = {
            gpio_config_.display_mosi,
            gpio_config_.display_clk,
            gpio_config_.display_dc,
            gpio_config_.display_rst,
            gpio_config_.display_backlight
        };

        for (int pin : pins) {
            if (!IsOutputPin(pin)) {
                ESP_LOGE(TAG, "Invalid display GPIO: %d", pin);
                return false;
            }
        }

        // CS is optional. -1 means the display has no CS pin.
        if (gpio_config_.display_cs != -1 &&
            !IsOutputPin(gpio_config_.display_cs)) {
            ESP_LOGE(TAG, "Invalid display CS GPIO: %d",
                     gpio_config_.display_cs);
            return false;
        }

        return true;
    }

    bool IsAudioConfigValid() const {
        const int pins[] = {
            gpio_config_.spk_bclk,
            gpio_config_.spk_lrck,
            gpio_config_.spk_dout,
            gpio_config_.mic_sck,
            gpio_config_.mic_ws,
            gpio_config_.mic_din
        };

        for (int pin : pins) {
            if (!GPIO_IS_VALID_GPIO(static_cast<gpio_num_t>(pin))) {
                ESP_LOGE(TAG, "Invalid audio GPIO: %d", pin);
                return false;
            }
        }

        const int output_pins[] = {
            gpio_config_.spk_bclk,
            gpio_config_.spk_lrck,
            gpio_config_.spk_dout,
            gpio_config_.mic_sck,
            gpio_config_.mic_ws
        };

        for (int pin : output_pins) {
            if (!IsOutputPin(pin)) {
                ESP_LOGE(TAG, "Audio GPIO cannot output: %d", pin);
                return false;
            }
        }

        return true;
    }

    bool InitializeSpi() {
        if (!IsDisplayConfigValid()) {
            ESP_LOGE(TAG, "Display GPIO configuration is invalid");
            return false;
        }

        spi_bus_config_t buscfg = {};

        buscfg.mosi_io_num = gpio_config_.display_mosi;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = gpio_config_.display_clk;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz =
            DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);

        esp_err_t err = spi_bus_initialize(
            SPI3_HOST,
            &buscfg,
            SPI_DMA_CH_AUTO
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SPI initialization failed: %s",
                     esp_err_to_name(err));
            return false;
        }

        spi_initialized_ = true;
        ESP_LOGI(TAG, "SPI initialized");
        return true;
    }

    bool InitializeLcdDisplay() {
        if (!spi_initialized_) {
            ESP_LOGE(TAG, "Cannot initialize LCD: SPI is unavailable");
            return false;
        }

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};

        // Support displays without a physical CS pin.
        io_config.cs_gpio_num =
            gpio_config_.display_cs < 0
                ? GPIO_NUM_NC
                : static_cast<gpio_num_t>(gpio_config_.display_cs);

        io_config.dc_gpio_num = gpio_config_.display_dc;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        esp_err_t err = esp_lcd_new_panel_io_spi(
            SPI3_HOST,
            &io_config,
            &panel_io
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD panel IO initialization failed: %s",
                     esp_err_to_name(err));
            return false;
        }

        esp_lcd_panel_dev_config_t panel_config = {};

        panel_config.reset_gpio_num = gpio_config_.display_rst;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        err = esp_lcd_new_panel_st7789(
            panel_io,
            &panel_config,
            &panel
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ST7789 driver initialization failed: %s",
                     esp_err_to_name(err));
            esp_lcd_panel_io_del(panel_io);
            return false;
        }

        err = esp_lcd_panel_reset(panel);

        if (err == ESP_OK) {
            err = esp_lcd_panel_init(panel);
        }

        if (err == ESP_OK) {
            err = esp_lcd_panel_invert_color(
                panel,
                DISPLAY_INVERT_COLOR
            );
        }

        if (err == ESP_OK) {
            err = esp_lcd_panel_swap_xy(
                panel,
                DISPLAY_SWAP_XY
            );
        }

        if (err == ESP_OK) {
            err = esp_lcd_panel_mirror(
                panel,
                DISPLAY_MIRROR_X,
                DISPLAY_MIRROR_Y
            );
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ST7789 panel setup failed: %s",
                     esp_err_to_name(err));
            esp_lcd_panel_del(panel);
            esp_lcd_panel_io_del(panel_io);
            return false;
        }

        display_ = new (std::nothrow) OttoEmojiDisplay(
            panel_io,
            panel,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X,
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY
        );

        if (display_ == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate Otto display");
            esp_lcd_panel_del(panel);
            esp_lcd_panel_io_del(panel_io);
            return false;
        }

        ESP_LOGI(TAG, "Otto ST7789 display initialized");
        return true;
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();

            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }

            app.ToggleChatState();
        });
    }

    void InitializeWebSocketControlServer() {
        if (ws_control_server_ != nullptr) {
            return;
        }

        ESP_LOGI(TAG, "Initialize WebSocket control server");

        ws_control_server_ =
            new (std::nothrow) WebSocketControlServer();

        if (ws_control_server_ == nullptr) {
            ESP_LOGE(TAG, "Cannot allocate WebSocket control server");
            return;
        }

        if (!ws_control_server_->Start(8080)) {
            ESP_LOGE(TAG, "Failed to start WebSocket control server");
            delete ws_control_server_;
            ws_control_server_ = nullptr;
            return;
        }

        ESP_LOGI(TAG,
                 "GPIO configuration page: http://<robot-ip>:8080/gpio");
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        vTaskDelay(pdMS_TO_TICKS(1000));
        InitializeWebSocketControlServer();
    }

public:
    OttoRobot()
        : gpio_config_(OttoGpioConfigManager::Load()),
          head_controller_(gpio_config_),
          boot_button_(BOOT_BUTTON_GPIO) {

        ESP_LOGI(TAG, "GPIO configuration loaded");

        InitializeButtons();

        if (InitializeSpi()) {
            if (InitializeLcdDisplay()) {
                ESP_LOGI(TAG, "Display ready");
            } else {
                ESP_LOGW(TAG,
                         "Display unavailable; Wi-Fi setup remains available");
            }
        } else {
            ESP_LOGW(TAG,
                     "SPI unavailable; Wi-Fi setup remains available");
        }

        backlight_ready_ = IsOutputPin(gpio_config_.display_backlight);

        if (backlight_ready_ && display_ != nullptr) {
            GetBacklight()->RestoreBrightness();
        }
    }

    AudioCodec* GetAudioCodec() override {
        int spk_bclk;
        int spk_lrck;
        int spk_dout;
        int mic_sck;
        int mic_ws;
        int mic_din;

        if (IsAudioConfigValid()) {
            spk_bclk = gpio_config_.spk_bclk;
            spk_lrck = gpio_config_.spk_lrck;
            spk_dout = gpio_config_.spk_dout;
            mic_sck = gpio_config_.mic_sck;
            mic_ws = gpio_config_.mic_ws;
            mic_din = gpio_config_.mic_din;
        } else {
            ESP_LOGW(TAG,
                     "Invalid audio GPIO config; using default GPIO pins");

            spk_bclk = AUDIO_I2S_SPK_GPIO_BCLK;
            spk_lrck = AUDIO_I2S_SPK_GPIO_LRCK;
            spk_dout = AUDIO_I2S_SPK_GPIO_DOUT;
            mic_sck = AUDIO_I2S_MIC_GPIO_SCK;
            mic_ws = AUDIO_I2S_MIC_GPIO_WS;
            mic_din = AUDIO_I2S_MIC_GPIO_DIN;
        }

        static NoAudioCodecSimplex* audio_codec = nullptr;

        if (audio_codec == nullptr) {
            audio_codec = new (std::nothrow) NoAudioCodecSimplex(
                AUDIO_INPUT_SAMPLE_RATE,
                AUDIO_OUTPUT_SAMPLE_RATE,
                static_cast<gpio_num_t>(spk_bclk),
                static_cast<gpio_num_t>(spk_lrck),
                static_cast<gpio_num_t>(spk_dout),
                static_cast<gpio_num_t>(mic_sck),
                static_cast<gpio_num_t>(mic_ws),
                static_cast<gpio_num_t>(mic_din)
            );
        }

        if (audio_codec == nullptr) {
            ESP_LOGE(TAG, "Failed to create audio codec");
            std::abort();
        }

        return audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        if (!backlight_ready_) {
            return nullptr;
        }

        static PwmBacklight backlight(
            static_cast<gpio_num_t>(gpio_config_.display_backlight),
            DISPLAY_BACKLIGHT_OUTPUT_INVERT
        );

        return &backlight;
    }
};

DECLARE_BOARD(OttoRobot);
