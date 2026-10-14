#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "hilily_display.h"
#include "hilily_home_client.h"
#include "hilily_music_player.h"
#include "mcp_server.h"
#if CONFIG_HILILY_BATTERY_MONITOR
#include "hilily_battery.h"
#endif
#if CONFIG_HILILY_VBUS_SENSE
#include <driver/gpio.h>
#endif

#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#define TAG "Esp32S3ZeroTft"

class Esp32S3ZeroTftBoard : public WifiBoard {
private:
    Button boot_button_;
    Button wake_button_;
    HiLilyDisplay* display_ = nullptr;

    void InitializeSpi() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DISPLAY_MOSI_PIN;
        bus_config.miso_io_num = GPIO_NUM_NC;
        bus_config.sclk_io_num = DISPLAY_CLK_PIN;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        // LVGL draws in short strips. Avoid reserving full-frame DMA memory.
        bus_config.max_transfer_sz = DISPLAY_WIDTH * 20 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));

        display_ = new HiLilyDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "ST7789 240x240 initialized (SPI2)");
    }

    void InitializeButtons() {
        // Both buttons activate the same conversation path. The external button
        // is active-low, using the internal pull-up configured by Button.
        auto on_short_press = [this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        };
        // A deliberate long press enters Wi-Fi provisioning after normal boot.
        auto on_long_press = [this]() {
            EnterWifiConfigMode();
        };
        boot_button_.OnClick(on_short_press);
        wake_button_.OnClick(on_short_press);
        boot_button_.OnLongPress(on_long_press);
        wake_button_.OnLongPress(on_long_press);
    }

public:
    Esp32S3ZeroTftBoard() : boot_button_(BOOT_BUTTON_GPIO), wake_button_(WAKE_BUTTON_GPIO) {
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        StartHiLilyHomeClient(display_, HILILY_WORKER_HOME_URL);
        auto& music = HiLilyMusicPlayer::GetInstance();
        music.Configure(display_, HILILY_WORKER_BASE_URL);
        // Device-local MCP commands. A gateway may invoke them directly without an LLM.
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.music.play",
            "Play music locally on Hi Lily. ESP32 calls Worker Master directly for MP3 and timed LRC.",
            PropertyList({Property("song", kPropertyTypeString).SetMaxLength(100),
                          Property("artist", kPropertyTypeString, std::string(""))}),
            [](const PropertyList& p) -> ToolResult {
                const auto& song=p["song"].value<std::string>();
                const auto& artist=p["artist"].value<std::string>();
                if (!HiLilyMusicPlayer::GetInstance().Play(song,artist))
                    return std::unexpected("Cannot queue music playback");
                return std::string("Music command queued on device");
            });
        mcp.AddTool("self.music.status", "Get the local Hi Lily online music playback state",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().StatusJson();
            });
        mcp.AddTool("self.music.pause", "Pause local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Pause();
            });
        mcp.AddTool("self.music.resume", "Resume local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Resume();
            });
        mcp.AddTool("self.music.stop", "Stop local Hi Lily music",
            PropertyList(), [](const PropertyList&) -> ReturnValue {
                return HiLilyMusicPlayer::GetInstance().Stop();
            });
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        // Separate I2S controllers: PCM5102A output and INMP441 input.
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            I2S_STD_SLOT_LEFT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_LEFT);
        return &audio_codec;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
#if CONFIG_HILILY_BATTERY_MONITOR
        static HiLilyBattery battery;
        if (!battery.ReadLevel(level)) return false;
#if CONFIG_HILILY_VBUS_SENSE
        static bool vbus_initialized = []() {
            gpio_config_t cfg = {};
            cfg.pin_bit_mask = 1ULL << VBUS_SENSE_GPIO;
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            return gpio_config(&cfg) == ESP_OK;
        }();
        charging = vbus_initialized && gpio_get_level(VBUS_SENSE_GPIO) == 1;
#else
        charging = false;  // Charging state unknown without a separate VBUS/CHRG sensor.
#endif
        discharging = !charging;
        return true;
#else
        (void)level; (void)charging; (void)discharging;
        return false;
#endif
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }
};

DECLARE_BOARD(Esp32S3ZeroTftBoard);
