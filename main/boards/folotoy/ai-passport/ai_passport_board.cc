#include "wifi_board.h"
#include "display/lcd_display.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"
#include "cw2017_battery_monitor.h"
#include "power_save_timer.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <button_adc.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "AiPassport"

// Physical keys share one ADC pin through a resistor ladder (see config.h).
enum {
    kAdcButtonUp = 0,
    kAdcButtonDown,
    kAdcButtonOk,
    kAdcButtonNum,
};

// ============================================================================
// Idle power policy.
//
// The Passport is a battery wearable, so it gives up the screen first and then
// stops altogether. Both steps are gated by Application::CanEnterSleepMode(),
// which PowerSaveTimer polls once per second, so an ongoing conversation or
// playback pushes the deadline back instead of interrupting it.
// ============================================================================

// Backlight off + CPU down-clocked this long after the last input.
static constexpr int kBacklightOffSeconds = 300;
// Deep sleep this long after the last input.
static constexpr int kDeepSleepSeconds = 900;
// ESP32-C3 caps at 160 MHz; passing a frequency (rather than -1) is what makes
// PowerSaveTimer allow DFS down to 40 MHz and tickless light sleep. It needs
// CONFIG_PM_ENABLE + CONFIG_FREERTOS_USE_TICKLESS_IDLE (see config.json).
static constexpr int kPowerSaveCpuMaxFreq = 160;
// Last-resort wake source if the key wakeup cannot be armed, so that a failed
// wake configuration cannot leave the device asleep until the battery dies.
static constexpr uint64_t kFallbackWakeupUs = 60ULL * 60ULL * 1000000ULL;

// ============================================================================
// ES8311 suspend, aligned register-for-register with the FoloToy AI Passport
// BSP (components/bsp/src/bsp_audio.c). The codec-dev disable path this board
// otherwise relies on is shallower (it leaves REG45 at 0x00) and is skipped
// entirely when the PCM path was never opened, so the terminal sequence writes
// the registers through the codec control interface instead.
// ============================================================================

struct Es8311RegValue {
    uint8_t reg;
    uint8_t value;
};

// The FoloToy BSP writes REG0E before the REG00 reset pulse in the middle of
// this list and then verifies REG0E == 0xFF. On this part bit 7 of REG0E does
// not latch: writing 0xFF (before or after the reset) reads back as 0x7F, so the
// verify below expects the value the hardware actually holds. Bits 0-6 - the
// power-down bits the sequence is after - do stick.
static constexpr Es8311RegValue kEs8311SuspendSequence[] = {
    {0x32, 0x00}, {0x17, 0x00}, {0x0E, 0xFF}, {0x12, 0x02},
    {0x14, 0x00}, {0x0D, 0xFA}, {0x15, 0x00}, {0x02, 0x10},
    {0x00, 0x00}, {0x00, 0x1F}, {0x01, 0x30}, {0x01, 0x00},
    {0x45, 0x01}, {0x0D, 0xFC}, {0x02, 0x00},
};

static constexpr Es8311RegValue kEs8311SuspendVerify[] = {
    {0x00, 0x1F}, {0x01, 0x00}, {0x0D, 0xFC},
    {0x0E, 0x7F}, {0x12, 0x02}, {0x45, 0x01},
};

static constexpr int kEs8311SuspendAttempts = 2;
static constexpr int kEs8311SuspendRetryMs = 5;

class AiPassportAudioCodec : public Es8311AudioCodec {
public:
    AiPassportAudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate,
                         int output_sample_rate, gpio_num_t mclk, gpio_num_t bclk,
                         gpio_num_t ws, gpio_num_t dout, gpio_num_t din, gpio_num_t pa_pin,
                         uint8_t es8311_addr)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                           mclk, bclk, ws, dout, din, pa_pin, es8311_addr) {}

    // Stops both I2S channels for the idle stage. The I2S standard-mode driver
    // takes an ESP_PM_APB_FREQ_MAX lock while a channel is enabled, and that
    // lock both pins the APB clock at 80 MHz and makes the PM subsystem skip
    // automatic light sleep entirely. Codec construction enables the channels
    // once and only esp_codec_dev_close() stops them again, so a board that has
    // not played anything since boot would otherwise never reach light sleep.
    void StopI2s() {
        if (dev_ != nullptr) {
            // The PCM path is still open. Closing it is what disables both
            // channels in the driver's own bookkeeping, so let that path do the
            // work instead of stopping them behind its back.
            EnableInput(false);
            EnableOutput(false);
            return;
        }
        SetI2sRunning(false);
    }

    // Counterpart for the wake path. Every audio path also restores the clocks
    // through esp_codec_dev_open -> data_if->enable(), so this only has to
    // cover waking up without opening the PCM path.
    void StartI2s() { SetI2sRunning(true); }

    // Terminal suspend for the way into deep sleep: write and verify the
    // ES8311 sleep sequence, then stop both I2S channels explicitly so the
    // clocks are off before the pins are released. Failures are logged but
    // never abort the shutdown - the pins and the panel still have to go down.
    void EnterDeepSleepState() {
        esp_err_t result = ESP_FAIL;
        for (int attempt = 1; attempt <= kEs8311SuspendAttempts; attempt++) {
            result = WriteSuspendSequence(attempt);
            if (result == ESP_OK) {
                break;
            }
            if (attempt < kEs8311SuspendAttempts) {
                vTaskDelay(pdMS_TO_TICKS(kEs8311SuspendRetryMs));
            }
        }
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "ES8311 suspend could not be verified, continuing shutdown");
        }
        StopI2s();
    }

private:
    void SetI2sRunning(bool running) {
        // TX before RX in both directions, matching the FoloToy BSP's
        // audio_disable_i2s_channels() / audio_prepare_i2s_reopen().
        const i2s_chan_handle_t channels[] = {tx_handle_, rx_handle_};
        int changed = 0;
        for (auto* channel : channels) {
            if (channel == nullptr) {
                continue;
            }
            // esp_codec_dev closes the PCM path, and with it both channels, once
            // the audio service idles out. Query the state first so a channel
            // that is already where we want it is not touched - the driver logs
            // an error for a redundant enable/disable.
            i2s_chan_info_t info = {};
            if (i2s_channel_get_info(channel, &info) != ESP_OK) {
                ESP_LOGW(TAG, "I2S channel state unavailable");
                continue;
            }
            if (info.is_enabled == running) {
                continue;
            }
            esp_err_t err = running ? i2s_channel_enable(channel)
                                    : i2s_channel_disable(channel);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "I2S channel %s failed: %s", running ? "start" : "stop",
                         esp_err_to_name(err));
                continue;
            }
            changed++;
        }
        ESP_LOGI(TAG, "I2S channels %s (%d changed)", running ? "started" : "stopped", changed);
    }

    esp_err_t WriteSuspendSequence(int attempt) {
        if (ctrl_if_ == nullptr || ctrl_if_->write_reg == nullptr ||
            ctrl_if_->read_reg == nullptr) {
            ESP_LOGE(TAG, "ES8311 control interface unavailable");
            return ESP_ERR_INVALID_STATE;
        }

        bool valid = true;
        for (const auto& item : kEs8311SuspendSequence) {
            uint8_t value = item.value;
            if (ctrl_if_->write_reg(ctrl_if_, item.reg, 1, &value, 1) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(TAG, "ES8311 suspend write failed (attempt %d, REG%02X)",
                         attempt, item.reg);
                valid = false;
            }
        }
        for (const auto& item : kEs8311SuspendVerify) {
            uint8_t actual = 0;
            if (ctrl_if_->read_reg(ctrl_if_, item.reg, 1, &actual, 1) != ESP_CODEC_DEV_OK ||
                actual != item.value) {
                ESP_LOGE(TAG, "ES8311 suspend verify failed (attempt %d, REG%02X "
                              "expected=0x%02X actual=0x%02X)",
                         attempt, item.reg, item.value, actual);
                valid = false;
            }
        }
        if (valid) {
            ESP_LOGI(TAG, "ES8311 suspended and verified (attempt %d)", attempt);
        }
        return valid ? ESP_OK : ESP_FAIL;
    }
};

class AiPassportBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button* adc_button_[kAdcButtonNum];
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    LcdDisplay* display_;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Cw2017BatteryMonitor* battery_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    bool deep_sleep_started_ = false;

    void InitializeCodecI2c() {
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

        // CW2017 fuel gauge is optional; a missing chip just disables battery UI.
        battery_ = new Cw2017BatteryMonitor(codec_i2c_bus_, BATTERY_CW2017_ADDR);
        battery_->Initialize();
    }

    void ChangeVolume(int delta) {
        auto codec = GetAudioCodec();
        auto volume = codec->output_volume() + delta;
        if (volume > 100) {
            volume = 100;
        }
        if (volume < 0) {
            volume = 0;
        }
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void ToggleChat() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState();
    }

    void InitializeButtons() {
        for (int i = 0; i < kAdcButtonNum; i++) {
            adc_button_[i] = nullptr;
        }

        // One ADC1 unit shared by all three ladder keys. AdcButton reuses the
        // handle when adc_config.adc_handle is non-null, so the same physical
        // pin can decode several keys without "adc1 is already in use".
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle_));

        button_adc_config_t adc_cfg = {};
        adc_cfg.adc_handle = &adc_handle_;
        adc_cfg.unit_id = ADC_UNIT_1;
        adc_cfg.adc_channel = ADC_CHANNEL_0;  // GPIO0

        adc_cfg.button_index = kAdcButtonUp;      // UP:   ~0 mV
        adc_cfg.min = BSP_ADC_BUTTON_UP_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_UP_MAX;
        adc_button_[kAdcButtonUp] = new AdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonDown;    // DOWN: ~300 mV
        adc_cfg.min = BSP_ADC_BUTTON_DOWN_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_DOWN_MAX;
        adc_button_[kAdcButtonDown] = new AdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonOk;      // OK:   ~595 mV
        adc_cfg.min = BSP_ADC_BUTTON_OK_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_OK_MAX;
        adc_button_[kAdcButtonOk] = new AdcButton(adc_cfg);

        // Button callbacks run on the button task; schedule all UI/audio
        // work onto the main task so LVGL and codec access stay on one thread.
        auto up = adc_button_[kAdcButtonUp];
        up->OnClick([this]() {
            Application::GetInstance().Schedule([this]() { ChangeVolume(10); });
        });
        up->OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(100);
                GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
            });
        });

        auto down = adc_button_[kAdcButtonDown];
        down->OnClick([this]() {
            Application::GetInstance().Schedule([this]() { ChangeVolume(-10); });
        });
        down->OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(0);
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            });
        });

        auto ok = adc_button_[kAdcButtonOk];
        ok->OnClick([this]() {
            Application::GetInstance().Schedule([this]() { ToggleChat(); });
        });

        // Cancel the idle countdown on press-down rather than on click, so a key
        // held for a long press also wakes the screen and the CPU immediately.
        for (auto* button : adc_button_) {
            button->OnPressDown([this]() { WakeUpPowerSaveTimer(); });
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // -1 -> software reset
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);

        // Panel-specific power/porch/gamma sequence for the ST7789P3 module
        // used on the Passport (copied from the original badge firmware).
        static const struct {
            uint8_t command;
            uint8_t data[16];
            uint8_t data_length;
            uint16_t delay_ms;
        } kSt7789P3InitCommands[] = {
            {0xB2, {0x05, 0x05, 0x00, 0x33, 0x33}, 5, 0},
            {0xB7, {0x35}, 1, 0},
            {0xBB, {0x21}, 1, 0},
            {0xC0, {0x2C}, 1, 0},
            {0xC2, {0x01}, 1, 0},
            {0xC3, {0x0B}, 1, 0},
            {0xC4, {0x20}, 1, 0},
            {0xC6, {0x0F}, 1, 0},
            {0xD0, {0xA7, 0xA1}, 2, 0},
            {0xD0, {0xA4, 0xA1}, 2, 0},
            {0xD6, {0xA1}, 1, 0},
            {0xE0, {0xD0, 0x04, 0x08, 0x0A, 0x09, 0x05, 0x2D, 0x43,
                    0x49, 0x09, 0x16, 0x15, 0x26, 0x2B}, 14, 0},
            {0xE1, {0xD0, 0x03, 0x09, 0x0A, 0x0A, 0x06, 0x2E, 0x44,
                    0x40, 0x3A, 0x15, 0x15, 0x26, 0x2A}, 14, 10},
        };
        for (const auto& cmd : kSt7789P3InitCommands) {
            esp_lcd_panel_io_tx_param(panel_io, cmd.command, cmd.data, cmd.data_length);
            if (cmd.delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(cmd.delay_ms));
            }
        }

        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        panel_ = panel;
        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void WakeUpPowerSaveTimer() {
        if (power_save_timer_ != nullptr) {
            power_save_timer_->WakeUp();
        }
    }

    // Pin holds are latched in the RTC domain and survive the deep-sleep reset
    // (see gpio_hold_en: the state is retained when the GPIO's power domain
    // goes off, including Deep-sleep events). Every hold taken before sleeping
    // therefore has to be dropped here, or the panel would stay dark and the
    // shared key node would stay pinned after a wake. Levels are rewritten
    // while the holds are still active so releasing them cannot glitch the LCD.
    void ReleaseDeepSleepHolds() {
        static_assert(sizeof(PANEL_PINS) == sizeof(PANEL_LEVELS),
                      "one safe level per LCD pin");
        gpio_deep_sleep_hold_dis();

        // Held by the sleep layer when it armed the GPIO0 key wakeup.
        esp_err_t err = gpio_hold_dis(GPIO_NUM_0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "GPIO%d hold release failed: %s", GPIO_NUM_0, esp_err_to_name(err));
        }

        for (size_t i = 0; i < sizeof(PANEL_PINS) / sizeof(PANEL_PINS[0]); i++) {
            gpio_num_t pin = PANEL_PINS[i];
            if (pin < 0) {
                continue;
            }
            gpio_config_t config = {
                .pin_bit_mask = 1ULL << (unsigned)pin,
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            if (gpio_config(&config) == ESP_OK) {
                gpio_set_level(pin, PANEL_LEVELS[i]);
            }
            err = gpio_hold_dis(pin);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "LCD GPIO%d hold release failed: %s", (int)pin,
                         esp_err_to_name(err));
            }
        }
        ESP_LOGI(TAG, "Deep-sleep pin holds released");
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(kPowerSaveCpuMaxFreq, kBacklightOffSeconds,
                                               kDeepSleepSeconds);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Idle %ds: backlight off, CPU down-clocked", kBacklightOffSeconds);
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(0);
            StopAudioI2s();
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Input: restoring backlight and CPU clock");
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
            StartAudioI2s();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            if (deep_sleep_started_) {
                return;
            }
            // The terminal sequence holds the LVGL lock and drives the codec, so
            // it has to run on the main task that owns both.
            Application::GetInstance().Schedule([this]() { EnterDeepSleep(); });
        });
        power_save_timer_->SetEnabled(true);
    }

    // Deep sleep is terminal: every step below is irreversible until the chip
    // resets, and the order matches the FoloToy BSP contract (battery, audio,
    // audio pins, I2C pins, panel). Individual failures are logged but do not
    // stop the remaining steps - the device must never be left awake with a
    // half-shut-down board.
    void EnterDeepSleep() {
        if (deep_sleep_started_) {
            return;
        }
        deep_sleep_started_ = true;

        ESP_LOGI(TAG, "Idle %ds: entering deep sleep, wake on any key", kDeepSleepSeconds);

        PrepareWakeupSource();
        battery_->EnterSleep();
        SuspendAudio();
        ReleasePins(AUDIO_PINS, sizeof(AUDIO_PINS) / sizeof(AUDIO_PINS[0]), "I2S");
        ReleasePins(I2C_PINS, sizeof(I2C_PINS) / sizeof(I2C_PINS[0]), "I2C");
        PrepareDisplayForDeepSleep();

        ESP_LOGI(TAG, "Entering deep sleep");
        esp_deep_sleep_start();

        // Deep sleep does not return. If it somehow does, the buses and pins are
        // already released and the panel is asleep, so restarting is the only
        // safe outcome.
        ESP_LOGE(TAG, "Deep sleep returned unexpectedly, restarting");
        esp_restart();
    }

    // All three keys pull the shared ADC node low, and only GPIO0-5 can wake the
    // ESP32-C3 from deep sleep (there is no EXT0/EXT1 on this chip).
    void PrepareWakeupSource() {
        gpio_config_t key_config = {
            .pin_bit_mask = 1ULL << GPIO_NUM_0,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&key_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "GPIO%d input config failed: %s", GPIO_NUM_0, esp_err_to_name(err));
        }

        // The first argument is a pin bit *mask*, not a pin number. Passing
        // GPIO_NUM_0 (value 0) is rejected as an invalid mask and would leave
        // the device in a deep sleep that no key can end.
        err = esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(
            1ULL << GPIO_NUM_0, ESP_GPIO_WAKEUP_GPIO_LOW);
        if (err != ESP_OK) {
            // A wakeup source is mandatory: without one the device would only
            // come back on a power cycle. A timed wake keeps it recoverable.
            ESP_LOGE(TAG, "GPIO wakeup failed (%s), falling back to a timed wake",
                     esp_err_to_name(err));
            esp_sleep_enable_timer_wakeup(kFallbackWakeupUs);
        }
    }

    void SuspendAudio() {
        auto* codec = static_cast<AiPassportAudioCodec*>(GetAudioCodec());
        if (codec == nullptr) {
            return;
        }

        // Let codec-dev update its own state first: with both directions off it
        // closes the PCM path. The forced register sequence runs afterwards, so
        // it also covers a board that never opened the PCM path at all.
        codec->EnableInput(false);
        codec->EnableOutput(false);
        codec->EnterDeepSleepState();
    }

    void StopAudioI2s() {
        if (auto* codec = static_cast<AiPassportAudioCodec*>(GetAudioCodec())) {
            codec->StopI2s();
        }
    }

    void StartAudioI2s() {
        if (auto* codec = static_cast<AiPassportAudioCodec*>(GetAudioCodec())) {
            codec->StartI2s();
        }
    }

    void ReleasePins(const gpio_num_t* pins, size_t count, const char* label) {
        uint64_t mask = 0;
        for (size_t i = 0; i < count; i++) {
            if (pins[i] >= 0) {
                mask |= 1ULL << (unsigned)pins[i];
            }
        }
        if (mask == 0) {
            return;
        }

        gpio_config_t config = {
            .pin_bit_mask = mask,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Releasing %s pins failed: %s", label, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "%s pins released as high-impedance inputs", label);
        }
    }

    // Terminal display step. Holding the LVGL lock both waits for the flush in
    // flight and blocks new ones while the panel and its pins are taken down.
    void PrepareDisplayForDeepSleep() {
        DisplayLockGuard lock(display_);
        if (!lock) {
            ESP_LOGE(TAG, "Display lock unavailable, taking the panel down anyway");
        }

        if (panel_ != nullptr) {
            esp_err_t err = esp_lcd_panel_disp_on_off(panel_, false);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Panel display-off failed: %s", esp_err_to_name(err));
            }
            err = esp_lcd_panel_disp_sleep(panel_, true);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Panel sleep-in failed: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "No panel handle, skipping panel sleep-in");
        }

        // Stop the backlight PWM at zero and hand the pin back to GPIO. The
        // board's PwmBacklight owns LEDC channel 0 of the low-speed mode.
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Backlight PWM stop failed: %s", esp_err_to_name(err));
        }

        static_assert(sizeof(PANEL_PINS) == sizeof(PANEL_LEVELS),
                      "one safe level per LCD pin");

        for (size_t i = 0; i < sizeof(PANEL_PINS) / sizeof(PANEL_PINS[0]); i++) {
            gpio_num_t pin = PANEL_PINS[i];
            if (pin < 0) {
                continue;
            }
            gpio_config_t config = {
                .pin_bit_mask = 1ULL << (unsigned)pin,
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            err = gpio_config(&config);
            if (err == ESP_OK) {
                err = gpio_set_level(pin, PANEL_LEVELS[i]);
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "LCD GPIO%d safe level failed: %s", (int)pin,
                         esp_err_to_name(err));
                continue;
            }
            err = gpio_hold_en(pin);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "LCD GPIO%d hold failed: %s", (int)pin, esp_err_to_name(err));
            }
        }
        gpio_deep_sleep_hold_en();
        ESP_LOGI(TAG, "Panel asleep, LCD pin levels held through deep sleep");
    }

    static constexpr gpio_num_t PANEL_PINS[] = {
        DISPLAY_SPI_CS_PIN, DISPLAY_SPI_SCK_PIN, DISPLAY_SPI_MOSI_PIN,
        DISPLAY_DC_PIN, DISPLAY_BACKLIGHT_PIN,
    };
    // The panel must not be selected while the chip sleeps, so CS stays high and
    // every other LCD line stays low.
    static constexpr uint32_t PANEL_LEVELS[] = {1, 0, 0, 0, 0};
    static constexpr gpio_num_t AUDIO_PINS[] = {
        AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
        AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
    };
    static constexpr gpio_num_t I2C_PINS[] = {
        AUDIO_CODEC_I2C_SDA_PIN, AUDIO_CODEC_I2C_SCL_PIN,
    };

public:
    AiPassportBoard() : display_(nullptr), battery_(nullptr) {
        ReleaseDeepSleepHolds();
        InitializeCodecI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
        InitializePowerSaveTimer();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static AiPassportAudioCodec audio_codec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!battery_ || !battery_->IsPresent()) {
            return false;
        }
        int soc = battery_->GetBatteryLevel();
        if (soc < 0) {
            return false;
        }
        level = soc;
        // CW2017 reports no charge state and the Passport has no charge-detect
        // GPIO, so report a plain (discharging) reading.
        charging = false;
        discharging = true;
        return true;
    }
};

DECLARE_BOARD(AiPassportBoard);
