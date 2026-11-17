#include "wifi_board.h"

#include "adc_battery_monitor.h"
#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/oled_display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "press_to_talk_mcp_tool.h"

#include <driver/i2c_master.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>

#include <algorithm>

#define TAG "OlighkerS3Oled242"

class OlighkerEsp32S3Oled242XzBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    sdmmc_card_t* sd_card_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    PressToTalkMcpTool* press_to_talk_tool_ = nullptr;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = DISPLAY_SDA_PIN;
        bus_config.scl_io_num = DISPLAY_SCL_PIN;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    uint8_t ProbeDisplayAddress() {
        constexpr uint8_t kDisplayAddresses[] = {0x3c, 0x3d};
        for (uint8_t address : kDisplayAddresses) {
            if (i2c_master_probe(display_i2c_bus_, address, 100) == ESP_OK) {
                ESP_LOGI(TAG, "OLED found at I2C address 0x%02x", address);
                return address;
            }
        }
        return 0;
    }

    void InitializeSsd1309Display() {
        const uint8_t display_address = ProbeDisplayAddress();
        if (display_address == 0) {
            ESP_LOGE(TAG, "SSD1309 OLED not found at 0x3c or 0x3d");
            display_ = new NoDisplay();
            return;
        }

        // SSD1309 uses the SSD1306-compatible command set needed by the IDF panel driver.
        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = display_address;
        io_config.scl_speed_hz = 400 * 1000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1309_config = {};
        ssd1309_config.height = DISPLAY_HEIGHT;
        panel_config.vendor_config = &ssd1309_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SSD1309-compatible OLED");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                   DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializePowerStatusPins() {
        gpio_config_t config = {};
        config.pin_bit_mask =
            (1ULL << BATTERY_CHARGE_STATUS_GPIO) | (1ULL << BATTERY_POWER_GOOD_GPIO);
        config.mode = GPIO_MODE_INPUT;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    void InitializeSdCard() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = SD_MOSI_GPIO;
        bus_config.miso_io_num = SD_MISO_GPIO;
        bus_config.sclk_io_num = SD_SCK_GPIO;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = 16 * 1024;

        esp_err_t err = spi_bus_initialize(SD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to initialize microSD SPI bus: %s", esp_err_to_name(err));
            return;
        }

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SD_SPI_HOST;
        host.max_freq_khz = SD_MAX_FREQUENCY_KHZ;

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.host_id = SD_SPI_HOST;
        slot_config.gpio_cs = SD_CS_GPIO;
        slot_config.gpio_cd = SDSPI_SLOT_NO_CD;
        slot_config.gpio_wp = GPIO_NUM_NC;
        slot_config.gpio_int = GPIO_NUM_NC;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = false;
        mount_config.max_files = 5;
        mount_config.allocation_unit_size = 16 * 1024;

        err =
            esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "microSD not mounted: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "microSD mounted at %s", SD_MOUNT_POINT);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (!press_to_talk_tool_ || !press_to_talk_tool_->IsPressToTalkEnabled()) {
                app.ToggleChatState();
            }
        });
        boot_button_.OnPressDown([this]() {
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                Application::GetInstance().StopListening();
            }
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            const int volume = std::min(codec->output_volume() + 10, 100);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            const int volume = std::max(codec->output_volume() - 10, 0);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeTools() {
        press_to_talk_tool_ = new PressToTalkMcpTool();
        press_to_talk_tool_->Initialize();
        static LampController lamp(LAMP_GPIO);
    }

public:
    OlighkerEsp32S3Oled242XzBoard()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1309Display();
        InitializePowerStatusPins();
        InitializeSdCard();
        InitializeButtons();
        InitializeTools();
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_SPK_SLOT,
                                               AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
                                               AUDIO_I2S_MIC_GPIO_DIN, AUDIO_I2S_MIC_SLOT);
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static AdcBatteryMonitor battery_monitor(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL,
                                                 BATTERY_UPPER_RESISTOR, BATTERY_LOWER_RESISTOR);
        level = battery_monitor.GetBatteryLevel();

        const bool external_power = gpio_get_level(BATTERY_POWER_GOOD_GPIO) == 0;
        const bool charge_active = gpio_get_level(BATTERY_CHARGE_STATUS_GPIO) == 0;
        charging = external_power && charge_active;
        discharging = !external_power;
        return true;
    }
};

DECLARE_BOARD(OlighkerEsp32S3Oled242XzBoard);
