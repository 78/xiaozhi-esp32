#include "wifi_board.h"

#include "adc_battery_monitor.h"
#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#ifdef DISPLAY_USE_ST7789
#include "backlight.h"
#include "display/lcd_display.h"
#else
#include "display/oled_display.h"
#endif
#include "lamp_controller.h"
#include "led/single_led.h"
#include "press_to_talk_mcp_tool.h"

#include <driver/i2c_master.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_ops.h>
#ifdef DISPLAY_USE_SH1106
#include <esp_lcd_panel_sh1106.h>
#endif
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <dirent.h>

#include <algorithm>
#include <cerrno>
#include <mutex>

#define TAG "OlighkerXz"

class OlighkerXzBoard : public WifiBoard {
private:
    enum class BatteryPowerState {
        kUnknown,
        kCharging,
        kChargeComplete,
        kExternalPowerIdle,
        kDischarging,
    };

#ifndef DISPLAY_USE_ST7789
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
#endif
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    sdmmc_card_t* sd_card_ = nullptr;
    esp_err_t sd_mount_result_ = ESP_ERR_INVALID_STATE;
    const char* sd_mount_stage_ = "SPI bus initialization";
    int sd_root_error_ = 0;
    esp_timer_handle_t sd_report_timer_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    PressToTalkMcpTool* press_to_talk_tool_ = nullptr;
    std::mutex battery_mutex_;
    BatteryPowerState battery_power_state_ = BatteryPowerState::kUnknown;
    int reported_battery_level_ = -1;
    int64_t battery_level_update_time_us_ = 0;

    int UpdateReportedBatteryLevel(int measured_level, BatteryPowerState power_state) {
        measured_level = std::clamp(measured_level, 0, 100);
        const int64_t now_us = esp_timer_get_time();

        if (reported_battery_level_ < 0) {
            reported_battery_level_ =
                power_state == BatteryPowerState::kChargeComplete ? 100 : measured_level;
            battery_power_state_ = power_state;
            battery_level_update_time_us_ = now_us;
            return reported_battery_level_;
        }

        if (power_state != battery_power_state_) {
            battery_power_state_ = power_state;
            battery_level_update_time_us_ = now_us;
        }

        if (power_state == BatteryPowerState::kChargeComplete) {
            reported_battery_level_ = 100;
            battery_level_update_time_us_ = now_us;
            return reported_battery_level_;
        }

        int step_interval_ms = 0;
        int direction = 0;
        if (power_state == BatteryPowerState::kCharging &&
            measured_level > reported_battery_level_) {
            step_interval_ms = BATTERY_CHARGE_STEP_INTERVAL_MS;
            direction = 1;
        } else if (power_state == BatteryPowerState::kDischarging &&
                   measured_level < reported_battery_level_) {
            step_interval_ms = BATTERY_DISCHARGE_STEP_INTERVAL_MS;
            direction = -1;
        }

        if (direction == 0) {
            return reported_battery_level_;
        }

        const int64_t elapsed_ms = (now_us - battery_level_update_time_us_) / 1000;
        const int available_steps = static_cast<int>(elapsed_ms / step_interval_ms);
        if (available_steps <= 0) {
            return reported_battery_level_;
        }

        const int difference = std::abs(measured_level - reported_battery_level_);
        const int applied_steps = std::min(available_steps, difference);
        reported_battery_level_ += direction * applied_steps;
        battery_level_update_time_us_ = now_us;
        return reported_battery_level_;
    }

#ifdef DISPLAY_USE_ST7789
    void InitializeLcdDisplay() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = LCD_SPI_MOSI_GPIO;
        bus_config.miso_io_num = GPIO_NUM_NC;
        bus_config.sclk_io_num = LCD_SPI_SCK_GPIO;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_SPI_CS_GPIO;
        io_config.dc_gpio_num = LCD_DC_GPIO;
        io_config.spi_mode = LCD_SPI_MODE;
        io_config.pclk_hz = LCD_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = LCD_RESET_GPIO;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (const esp_err_t err = esp_lcd_panel_init(panel_); err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize ST7789 LCD: %s", esp_err_to_name(err));
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_LOGI(TAG, "ST7789 LCD initialized (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        GetBacklight()->RestoreBrightness();
    }
#else
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
                ESP_LOGI(TAG, "%s OLED found at I2C address 0x%02x", DISPLAY_CONTROLLER_NAME,
                         address);
                return address;
            }
        }
        return 0;
    }

    void InitializeOledDisplay() {
        const uint8_t display_address = ProbeDisplayAddress();
        if (display_address == 0) {
            ESP_LOGE(TAG, "%s OLED not found at 0x3c or 0x3d", DISPLAY_CONTROLLER_NAME);
            display_ = new NoDisplay();
            return;
        }

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

#ifdef DISPLAY_USE_SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        // The SSD1309 modules use the SSD1306-compatible command set.
        esp_lcd_panel_ssd1306_config_t ssd1306_config = {};
        ssd1306_config.height = DISPLAY_HEIGHT;
        panel_config.vendor_config = &ssd1306_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize %s OLED", DISPLAY_CONTROLLER_NAME);
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                   DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }
#endif

    void InitializePowerStatusPins() {
        gpio_config_t config = {};
        config.pin_bit_mask =
            (1ULL << BATTERY_CHARGE_STATUS_GPIO) | (1ULL << BATTERY_POWER_GOOD_GPIO);
        config.mode = GPIO_MODE_INPUT;
        // BQ24074 CHG# and PGOOD# are open-drain, active-low outputs.
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    esp_err_t MountSdCard() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = SD_MOSI_GPIO;
        bus_config.miso_io_num = SD_MISO_GPIO;
        bus_config.sclk_io_num = SD_SCK_GPIO;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = 16 * 1024;

        esp_err_t err = spi_bus_initialize(SD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            return err;
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

        sd_mount_stage_ = "card initialization or FAT mount";
        err =
            esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card_);
        if (err != ESP_OK) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_free(SD_SPI_HOST));
        }
        return err;
    }

    void LogSdCardResult() const {
        if (sd_mount_result_ != ESP_OK) {
            ESP_LOGW(TAG, "microSD not mounted (%s): %s (0x%x)", sd_mount_stage_,
                     esp_err_to_name(sd_mount_result_), static_cast<unsigned>(sd_mount_result_));
            return;
        }
        ESP_LOGI(TAG, "microSD mounted at %s (SPI limit %d kHz)", SD_MOUNT_POINT,
                 SD_MAX_FREQUENCY_KHZ);
        if (sd_root_error_ == 0) {
            ESP_LOGI(TAG, "microSD root directory read OK");
        } else {
            ESP_LOGW(TAG, "microSD root directory read failed (errno=%d)", sd_root_error_);
        }
    }

    void InitializeSdCard() {
        sd_mount_result_ = MountSdCard();
        if (sd_mount_result_ == ESP_OK) {
            // Read at most one directory entry. End of an empty directory is also success.
            if (DIR* root = opendir(SD_MOUNT_POINT)) {
                errno = 0;
                if (readdir(root) == nullptr) {
                    sd_root_error_ = errno;
                }
                closedir(root);
            } else {
                sd_root_error_ = errno;
            }
        }
        LogSdCardResult();

        // Repeat the saved result after the USB serial console has had time to reconnect.
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = [](void* arg) {
            auto* board = static_cast<OlighkerXzBoard*>(arg);
            board->LogSdCardResult();
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_delete(board->sd_report_timer_));
        };
        timer_args.arg = this;
        timer_args.name = "sd_mount_report";
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_create(&timer_args, &sd_report_timer_)) ==
            ESP_OK) {
            constexpr uint64_t kSdReportDelayUs = 8'000'000;
            if (ESP_ERROR_CHECK_WITHOUT_ABORT(
                    esp_timer_start_once(sd_report_timer_, kSdReportDelayUs)) != ESP_OK) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_delete(sd_report_timer_));
            }
        }
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
    OlighkerXzBoard()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
#ifdef DISPLAY_USE_ST7789
        InitializeLcdDisplay();
#else
        InitializeDisplayI2c();
        InitializeOledDisplay();
#endif
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

#ifdef DISPLAY_USE_ST7789
    Backlight* GetBacklight() override {
        static PwmBacklight backlight(LCD_BACKLIGHT_GPIO, LCD_BACKLIGHT_INVERT);
        return &backlight;
    }
#endif

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        std::lock_guard<std::mutex> lock(battery_mutex_);
        static AdcBatteryMonitor battery_monitor(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL,
                                                 BATTERY_UPPER_RESISTOR, BATTERY_LOWER_RESISTOR);
        const int measured_level = battery_monitor.GetBatteryLevel();

        const bool external_power = gpio_get_level(BATTERY_POWER_GOOD_GPIO) == 0;
        const bool charge_active = gpio_get_level(BATTERY_CHARGE_STATUS_GPIO) == 0;
        charging = external_power && charge_active;
        discharging = !external_power;

        // BQ24074 releases CHG# when charge termination is reached, which also turns off the
        // hardware charge LED. ADC/resistor tolerance can still leave the estimated capacity
        // below 100%, so align the reported level with the charger's definitive done state.
        // Keep a top-of-charge guard to avoid reporting 100% for an absent battery or a fault.
        BatteryPowerState power_state = BatteryPowerState::kExternalPowerIdle;
        if (discharging) {
            power_state = BatteryPowerState::kDischarging;
        } else if (charging) {
            power_state = BatteryPowerState::kCharging;
        } else if (measured_level >= BATTERY_CHARGE_COMPLETE_MIN_LEVEL) {
            power_state = BatteryPowerState::kChargeComplete;
        }

        level = UpdateReportedBatteryLevel(measured_level, power_state);
        return true;
    }
};

DECLARE_BOARD(OlighkerXzBoard);
