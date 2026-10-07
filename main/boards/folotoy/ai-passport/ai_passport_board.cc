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
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <button_adc.h>
#include <wifi_manager.h>
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
// Idle power policy: three stages, each counted from the last key press.
//
//     60 s    the backlight drops to kDimBrightness; nothing else changes
//    360 s    screen and codec off, CPU down-clocked - the "soft sleep" stage
//   2160 s    deep sleep, entered from the soft-sleep stage as a fallback
//
// All three are gated by Application::CanEnterSleepMode(), which PowerSaveTimer
// polls once per second, so an ongoing conversation or playback pushes the
// deadline back instead of interrupting it. The NVS "sleep_mode" flag the
// provisioning page writes disables all three at once.
//
// The soft-sleep stage is deliberately shallow: the CPU keeps running and DFS
// only drops it to kStandbyCpuMinFreq. That is affordable because the
// deep-sleep fallback caps how long the stage can last - half an hour at the
// ~20 mA it draws is about 10 mAh of the 520 mAh cell, around 2% of a charge.
// Pausing the renderer (lvgl_port_stop) and letting the chip enter automatic
// light sleep would take the stage below 1 mAh in the same window, but both need
// hardware validation of their own; both are left for a follow-up.
//
// The radio is deliberately left up. Stopping the station is worth about 10 mA,
// but the device would then be unreachable for the whole window: no server push,
// a Wi-Fi reconnect on every wake, and - measured on hardware - a protocol that
// keeps retrying into a user-visible error alert (and a notification sound) once
// the application is idle again. Keeping the link costs roughly 1% of a charge
// per cycle and buys an instant wake and a reachable device. A longer window
// would change that arithmetic: over eight hours the radio is worth about a
// third of the cell, and turning it off would have to come with a way to quiet
// the protocol while it is down.
//
// The fallback is what makes the trade-off work: because it is reached from the
// soft-sleep stage, the device still ends up at deep-sleep current within the
// hour, so a forgotten Passport does not flatten its cell overnight.
// ============================================================================

// Backlight drops to kDimBrightness this long after the last input.
static constexpr int kDimSeconds = 60;
// Screen, codec and CPU down this long after the last input.
static constexpr int kSoftSleepSeconds = 360;
// Deep sleep this long after the last input.
static constexpr int kDeepSleepSeconds = 2160;
// PowerSaveTimer counts the fallback from the soft-sleep deadline rather than
// from the last input.
static constexpr int kDeepSleepAfterSoftSleepTicks = kDeepSleepSeconds - kSoftSleepSeconds;
// Dim level in percent. A display already at or below it is left alone.
static constexpr int kDimBrightness = 10;
// Safety net for the wake-key guard. The guard itself ends when the press that
// woke the device finishes, so this only bounds a release event that never
// arrives, or a key that is stuck down.
static constexpr int kWakeKeyGuardMs = 10000;
// ESP32-C3 caps at 160 MHz.
static constexpr int kCpuMaxFreq = 160;
// Standby clock. DFS does not stop the tick, which is what keeps the three
// deadlines above on wall-clock time; automatic light sleep is what would break
// them (see SetStandbyClock).
static constexpr int kStandbyCpuMinFreq = 40;
// Last-resort wake source if the key wakeup cannot be armed, so that a failed
// wake configuration cannot leave the device asleep until the battery dies.
static constexpr uint64_t kFallbackWakeupUs = 60ULL * 60ULL * 1000000ULL;

// ============================================================================
// ES8311 suspend, aligned register-for-register with the FoloToy AI Passport
// BSP (components/bsp/src/bsp_audio.c). The codec-dev disable path this board
// otherwise relies on is shallower (it leaves REG45 at 0x00) and is skipped
// entirely when the PCM path was never opened, so the terminal sequence writes
// the registers through a board-owned I2C device handle instead. The BSP owns
// its own control interface for the same reason; this keeps the sequence in the
// board layer rather than widening what the shared codec class exposes.
// ============================================================================

struct Es8311RegValue {
    uint8_t reg;
    uint8_t value;
};

// The FoloToy BSP writes REG0E before the REG00 reset pulse in the middle of
// this list and checks bits 6:0 of it (bsp_es8311_sleep_check.c). Bit 7 of that
// register does not latch on this part, so writing the vendor value 0xFF reads
// back as 0x7F - the check below expects that, and the power-down bits 0-6 the
// sequence is after do stick.
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
static constexpr int kEs8311I2cTimeoutMs = 100;

// ES8311_CODEC_DEFAULT_ADDR is the 8-bit form (0x30); an i2c_device_config_t takes
// the 7-bit address. esp_codec_dev shifts it the same way when it builds the codec's
// own handle (audio_codec_ctrl_i2c.c: device_address = i2c_cfg->addr >> 1), so the
// board has to as well - an unshifted address talks to nothing and every register
// access fails silently.
static constexpr uint8_t kEs8311I2cAddress = AUDIO_CODEC_ES8311_ADDR >> 1;

// The deep-sleep path needs the I2S channel handles before the pins are released,
// and those are protected on AudioCodec. Everything else it touches is public, so
// stopping the channels is the whole subclass. It matters because codec
// construction enables both channels once and only esp_codec_dev_close() stops
// them again: a board that never played anything since boot would otherwise reach
// the pin release with the clocks still running, leaving the codec driving inputs
// whose master has gone quiet.
class AiPassportAudioCodec : public Es8311AudioCodec {
public:
    AiPassportAudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate,
                         int output_sample_rate, gpio_num_t mclk, gpio_num_t bclk,
                         gpio_num_t ws, gpio_num_t dout, gpio_num_t din, gpio_num_t pa_pin,
                         uint8_t es8311_addr)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                           mclk, bclk, ws, dout, din, pa_pin, es8311_addr) {}

    void StopI2s() {
        // TX before RX, matching the FoloToy BSP's audio_disable_i2s_channels().
        const i2s_chan_handle_t channels[] = {tx_handle_, rx_handle_};
        int changed = 0;
        for (auto* channel : channels) {
            if (channel == nullptr) {
                continue;
            }
            // Query the state first: esp_codec_dev has usually closed the PCM
            // path, and with it both channels, by the time the audio service idles
            // out. The driver logs an error for a redundant disable.
            i2s_chan_info_t info = {};
            if (i2s_channel_get_info(channel, &info) != ESP_OK) {
                ESP_LOGW(TAG, "I2S channel state unavailable");
                continue;
            }
            if (!info.is_enabled) {
                continue;
            }
            esp_err_t err = i2s_channel_disable(channel);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "I2S channel stop failed: %s", esp_err_to_name(err));
                continue;
            }
            changed++;
        }
        ESP_LOGI(TAG, "I2S channels stopped (%d changed)", changed);
    }
};

// AdcButton with the two things the shared Button wrapper does not forward: the
// end of a press sequence and the current key level. Both are what let the board
// drop exactly one press - the one that woke it - instead of guessing with a time
// window.
class PassportAdcButton : public AdcButton {
public:
    using AdcButton::AdcButton;

    // BUTTON_PRESS_END is emitted after the sequence's click or long press, so a
    // guard released here has already filtered those out.
    void OnPressEnd(std::function<void()> callback) {
        on_press_end_ = callback;
        iot_button_register_cb(button_handle_, BUTTON_PRESS_END, nullptr,
                               [](void* handle, void* usr_data) {
                                   auto* button = static_cast<PassportAdcButton*>(usr_data);
                                   if (button->on_press_end_) {
                                       button->on_press_end_();
                                   }
                               },
                               this);
    }

    // Samples the ladder on demand. Used once at boot: the key that woke the
    // device out of deep sleep is still held while the component starts, so this
    // separates that press from a wake whose key was already released.
    bool IsKeyDown() const {
        return iot_button_get_key_level(button_handle_) == BUTTON_ACTIVE;
    }

private:
    std::function<void()> on_press_end_;
};

class AiPassportBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    PassportAdcButton* adc_button_[kAdcButtonNum];
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    LcdDisplay* display_;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Cw2017BatteryMonitor* battery_;
    // Board-owned handle for the terminal ES8311 suspend sequence. The codec has
    // its own handle through esp_codec_dev; this one is only used once the PCM
    // path is closed and the chip is about to be powered down.
    i2c_master_dev_handle_t es8311_handle_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    // Set and cleared on the main task, read from the button task. It is one
    // byte, so the worst case is a wake press whose actions are not dropped.
    bool soft_sleep_active_ = false;
    bool deep_sleep_started_ = false;
    int soft_sleep_ticks_ = 0;
    // Set while the press that woke the device is still in flight, so that press
    // cannot also change the volume or open the audio channel. The guard ends with
    // that press sequence rather than with a fixed window: the component grades a
    // long press 2 s after the press started, which is past any window short
    // enough to leave a normal press alone.
    bool wake_key_guard_ = false;
    // esp_timer_get_time() deadline the guard cannot outlive (kWakeKeyGuardMs).
    int64_t wake_key_guard_until_us_ = 0;
    // Set from the button task on every key press. PowerSaveTimer resets its own
    // counter on the same press, but both stage transitions are scheduled onto the
    // main task and can still run just before the wake handler does. Without this
    // the device would flicker into soft sleep - or fall asleep - on a key press
    // that lands in that window.
    int64_t last_key_us_ = 0;

    // Guards the stage transitions against a key press that arrived while they
    // were queued: PowerSaveTimer has already forgiven that press, but the wake
    // handler scheduled behind them has not run yet.
    bool IdleForAtLeast(int seconds) const {
        return esp_timer_get_time() - last_key_us_ >= (int64_t)seconds * 1000000;
    }

    // True while a key event should be dropped rather than acted on. Keys never
    // stop waking the device - only the actions of the wake press are held back.
    bool KeyEventsBlocked() const {
        return wake_key_guard_ && esp_timer_get_time() < wake_key_guard_until_us_;
    }

    // Arms the guard for a press that is about to wake the device. The deadline
    // keeps a missed sequence end from leaving the keys dead.
    void ArmWakeKeyGuard() {
        wake_key_guard_ = true;
        wake_key_guard_until_us_ = esp_timer_get_time() + (int64_t)kWakeKeyGuardMs * 1000;
    }

    // Ends the guard with the press sequence that owns it. Called from PRESS_END,
    // so the click and long press of that press have already been filtered out.
    void EndWakeKeyGuard() {
        if (!wake_key_guard_) {
            return;
        }
        wake_key_guard_ = false;
        ESP_LOGI(TAG, "Wake key sequence finished, key actions active again");
    }

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

        i2c_device_config_t es8311_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = kEs8311I2cAddress,
            .scl_speed_hz = 100 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(codec_i2c_bus_, &es8311_cfg, &es8311_handle_));

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

    // A live link is only needed by the ToggleChat() paths that would open the
    // audio channel. Provisioning is entered from kDeviceStateStarting and the
    // Wi-Fi-config screen uses the key to toggle the speaker test, so gating the
    // key on the station would make both unreachable.
    bool ToggleChatNeedsNetwork() const {
        const auto state = Application::GetInstance().GetDeviceState();
        return state != kDeviceStateStarting && state != kDeviceStateWifiConfiguring &&
               state != kDeviceStateAudioTesting;
    }

    void InitializeButtons() {
        for (int i = 0; i < kAdcButtonNum; i++) {
            adc_button_[i] = nullptr;
        }

        // Waking from the deep-sleep fallback reboots the chip while the key is
        // still held. The button component starts a few hundred milliseconds later,
        // registers that press, and grades it a long press once it has been down for
        // the component's 2 s threshold - UP would jump to full volume, DOWN would
        // mute. So that press has to be dropped as a whole. Only a boot from sleep
        // can have one: on a cold boot OK during kDeviceStateStarting is the
        // provisioning path and has to keep working.
        const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
        const bool wake_boot =
            (wake_causes & ((1u << ESP_SLEEP_WAKEUP_GPIO) | (1u << ESP_SLEEP_WAKEUP_TIMER))) != 0;

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
        adc_button_[kAdcButtonUp] = new PassportAdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonDown;    // DOWN: ~300 mV
        adc_cfg.min = BSP_ADC_BUTTON_DOWN_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_DOWN_MAX;
        adc_button_[kAdcButtonDown] = new PassportAdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonOk;      // OK:   ~595 mV
        adc_cfg.min = BSP_ADC_BUTTON_OK_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_OK_MAX;
        adc_button_[kAdcButtonOk] = new PassportAdcButton(adc_cfg);

        if (wake_boot) {
            // The wake press is still down here, so the key level tells a real wake
            // press from a wake whose key was released before the component started.
            // Arming the guard for the second kind would only swallow the next,
            // genuine press.
            bool wake_press_down = false;
            for (auto* button : adc_button_) {
                if (button->IsKeyDown()) {
                    wake_press_down = true;
                    break;
                }
            }
            if (wake_press_down) {
                ArmWakeKeyGuard();
                ESP_LOGI(TAG,
                         "Woke from deep sleep (causes 0x%02x) with a key down, "
                         "holding key actions until it comes up",
                         (unsigned)wake_causes);
            } else {
                ESP_LOGI(TAG, "Woke from deep sleep (causes 0x%02x), wake key already released",
                         (unsigned)wake_causes);
            }
        }

        // Button callbacks run on the button task; schedule all UI/audio
        // work onto the main task so LVGL and codec access stay on one thread.
        auto up = adc_button_[kAdcButtonUp];
        up->OnClick([this]() {
            if (KeyEventsBlocked()) return;
            Application::GetInstance().Schedule([this]() { ChangeVolume(10); });
        });
        up->OnLongPress([this]() {
            if (KeyEventsBlocked()) return;
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(100);
                GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
            });
        });

        auto down = adc_button_[kAdcButtonDown];
        down->OnClick([this]() {
            if (KeyEventsBlocked()) return;
            Application::GetInstance().Schedule([this]() { ChangeVolume(-10); });
        });
        down->OnLongPress([this]() {
            if (KeyEventsBlocked()) return;
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(0);
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            });
        });

        auto ok = adc_button_[kAdcButtonOk];
        ok->OnClick([this]() {
            if (KeyEventsBlocked()) return;
            Application::GetInstance().Schedule([this]() {
                // Opening the audio channel with the network down would only
                // raise an error alert, so show the ordinary "connecting" hint
                // instead. Covers a wake from soft sleep as well as a link that
                // dropped on its own with the screen off. Provisioning and the
                // Wi-Fi-config speaker test do not open one.
                if (ToggleChatNeedsNetwork() && !WifiManager::GetInstance().IsConnected()) {
                    GetDisplay()->ShowNotification(Lang::Strings::CONNECTING);
                    return;
                }
                ToggleChat();
            });
        });

        // Cancel the idle countdown on press-down rather than on click, so a key
        // held for a long press also wakes the screen and the CPU immediately.
        // Press-down itself is never blocked: it is what wakes the device.
        for (auto* button : adc_button_) {
            button->OnPressDown([this]() {
                last_key_us_ = esp_timer_get_time();
                // The press that brings the device out of soft sleep is dropped as
                // a whole, long press included.
                if (soft_sleep_active_) {
                    ArmWakeKeyGuard();
                }
                WakeUpPowerSaveTimer();
            });
            // Not released on press-up: the click that belongs to the wake press is
            // graded a tick after it, so the guard has to outlive the release and
            // only end with the sequence.
            button->OnPressEnd([this]() { EndWakeKeyGuard(); });
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
        // cpu_max_freq = -1 keeps PowerSaveTimer out of the wake-word and PM
        // logic: both belong to the soft-sleep stage below, and PowerSaveTimer
        // would otherwise apply them at the dim stage (and with automatic light
        // sleep enabled, which is deliberately not used here).
        power_save_timer_ = new PowerSaveTimer(-1, kDimSeconds, kSoftSleepSeconds);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Idle %ds: backlight to %d%%", kDimSeconds, kDimBrightness);
            auto* backlight = GetBacklight();
            if (backlight != nullptr && backlight->brightness() > kDimBrightness) {
                backlight->SetBrightness(kDimBrightness);
            }
        });
        // PowerSaveTimer::WakeUp() runs on the esp_timer task because the button
        // task calls it, so everything that touches LVGL, the codec or Wi-Fi has
        // to go through the main task.
        power_save_timer_->OnExitSleepMode([this]() {
            Application::GetInstance().Schedule([this]() { WakeFromIdle(); });
        });
        power_save_timer_->OnShutdownRequest([this]() { OnSoftSleepDeadline(); });
        power_save_timer_->SetEnabled(true);
    }

    // PowerSaveTimer::WakeUp() fires on every key press-down. It only invokes the
    // exit callback while it is in its sleep stage, which is exactly from the dim
    // deadline on, so this one path serves both stages.
    void WakeFromIdle() {
        if (soft_sleep_active_) {
            ExitSoftSleep();
            return;
        }
        GetBacklight()->RestoreBrightness();
    }

    // PowerSaveTimer calls this once per second from the soft-sleep deadline on,
    // and keeps calling it every second after that - both branches have to be
    // idempotent.
    void OnSoftSleepDeadline() {
        // Past its sleep deadline PowerSaveTimer stops checking
        // CanEnterSleepMode(), so a conversation started by the wake word - which
        // presses no key and therefore never resets the tick counter - would
        // otherwise keep counting and be cut off by the deep-sleep fallback. Hold
        // the countdown here instead and let the deadline pass.
        if (!Application::GetInstance().CanEnterSleepMode()) {
            return;
        }
        if (deep_sleep_started_) {
            return;
        }
        if (!soft_sleep_active_) {
            // The stage drives the panel, the codec and Wi-Fi, all of which
            // belong to the main task (and LVGL to the lock it owns).
            Application::GetInstance().Schedule([this]() { EnterSoftSleep(); });
            return;
        }
        if (++soft_sleep_ticks_ >= kDeepSleepAfterSoftSleepTicks) {
            Application::GetInstance().Schedule([this]() { EnterDeepSleep(); });
        }
    }

    void EnterSoftSleep() {
        if (soft_sleep_active_ || deep_sleep_started_) {
            return;
        }
        if (!Application::GetInstance().CanEnterSleepMode() || !IdleForAtLeast(kSoftSleepSeconds)) {
            return;
        }
        soft_sleep_active_ = true;
        ESP_LOGI(TAG, "Idle %ds: screen off, codec off, CPU down-clocked", kSoftSleepSeconds);

        // Order matters. The audio input task re-enables the codec on every read
        // it starts, so switching the wake word off first and leaving the codec
        // to AudioService is what actually closes the PCM path - it does that on
        // its own once the input has been idle for AUDIO_POWER_TIMEOUT_MS.
        // Calling EnableInput(false) from here would be undone by the next read.
        Application::GetInstance().GetAudioService().EnableWakeWordDetection(false);

        SleepPanel();
        GetBacklight()->SetBrightness(0);
        SetStandbyClock(true);
    }

    void ExitSoftSleep() {
        if (!soft_sleep_active_) {
            GetBacklight()->RestoreBrightness();
            return;
        }
        soft_sleep_active_ = false;
        soft_sleep_ticks_ = 0;
        ESP_LOGI(TAG, "Input: waking from soft sleep");

        WakePanel();
        GetBacklight()->RestoreBrightness();

        // The idle branch of the state machine does not run again on its own, so
        // the wake word has to be switched back on here or the device would never
        // hear it again for the rest of this boot.
        Application::GetInstance().GetAudioService().EnableWakeWordDetection(true);

        SetStandbyClock(false);
    }

    // Holding the LVGL lock waits out the flush in flight and blocks new ones
    // while the panel goes down.
    void SleepPanel() {
        DisplayLockGuard lock(display_);
        if (!lock) {
            ESP_LOGW(TAG, "Display lock unavailable, sleeping the panel anyway");
        }
        if (panel_ == nullptr) {
            ESP_LOGW(TAG, "No panel handle, skipping panel sleep-in");
            return;
        }
        esp_err_t err = esp_lcd_panel_disp_on_off(panel_, false);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Panel display-off failed: %s", esp_err_to_name(err));
        }
        err = esp_lcd_panel_disp_sleep(panel_, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Panel sleep-in failed: %s", esp_err_to_name(err));
        }
    }

    void WakePanel() {
        if (panel_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(display_);
        if (!lock) {
            ESP_LOGW(TAG, "Display lock unavailable, waking the panel anyway");
        }
        esp_err_t err = esp_lcd_panel_disp_sleep(panel_, false);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Panel sleep-out failed: %s", esp_err_to_name(err));
        }
        err = esp_lcd_panel_disp_on_off(panel_, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Panel display-on failed: %s", esp_err_to_name(err));
        }
        // The panel keeps its GRAM through sleep-in, but a full repaint is the
        // only way to be sure the first frame after waking matches LVGL's state.
        lv_obj_invalidate(lv_screen_active());
    }

    // The soft-sleep stage down-clocks the CPU and deliberately stops there. The
    // countdown behind all three deadlines runs on an esp_timer with
    // skip_unhandled_events set, and such a timer does not wake the chip out of
    // light sleep - so turning light sleep on here would silently stretch 60 /
    // 360 / 2160 into wall-clock times nobody asked for. DFS has no such effect.
    void SetStandbyClock(bool standby) {
        esp_pm_config_t pm_config = {
            .max_freq_mhz = kCpuMaxFreq,
            .min_freq_mhz = standby ? kStandbyCpuMinFreq : kCpuMaxFreq,
            .light_sleep_enable = false,
        };
        esp_err_t err = esp_pm_configure(&pm_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PM reconfiguration failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "CPU %s", standby ? "down-clocked to 40 MHz" : "back to full speed");
        }
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
        // Same reason as OnSoftSleepDeadline(): never fire while the device is
        // back in a conversation, and never fire on a key press that arrived
        // while this transition was queued.
        if (!Application::GetInstance().CanEnterSleepMode() || !IdleForAtLeast(kDeepSleepSeconds)) {
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
        // Start from a clean slate. Automatic light sleep (CONFIG_PM_ENABLE with
        // light_sleep_enable) programs the RTC timer for the next scheduled
        // event, and that wake source is still armed when deep sleep starts - so
        // the chip would wake up again on the next OS tick instead of staying
        // asleep. Measured on hardware: the wake happened about a second after
        // entering deep sleep and reported ESP_SLEEP_WAKEUP_TIMER with an empty
        // GPIO wake status.
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

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
        SuspendEs8311();
        // Last, so the clocks are already off when the pins are released below.
        codec->StopI2s();
    }

    esp_err_t WriteEs8311Reg(uint8_t reg, uint8_t value) {
        if (es8311_handle_ == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        const uint8_t buffer[2] = {reg, value};
        return i2c_master_transmit(es8311_handle_, buffer, sizeof(buffer), kEs8311I2cTimeoutMs);
    }

    esp_err_t ReadEs8311Reg(uint8_t reg, uint8_t* value) {
        if (es8311_handle_ == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        return i2c_master_transmit_receive(es8311_handle_, &reg, 1, value, 1,
                                           kEs8311I2cTimeoutMs);
    }

    // One pass over the BSP's suspend sequence, then a readback of the registers
    // that have to stick. Runs with the PCM path closed and the audio service
    // idle, so the codec driver is not touching the same registers.
    esp_err_t WriteEs8311SuspendSequence(int attempt) {
        if (es8311_handle_ == nullptr) {
            ESP_LOGE(TAG, "ES8311 device handle unavailable");
            return ESP_ERR_INVALID_STATE;
        }

        bool valid = true;
        for (const auto& item : kEs8311SuspendSequence) {
            if (WriteEs8311Reg(item.reg, item.value) != ESP_OK) {
                ESP_LOGE(TAG, "ES8311 suspend write failed (attempt %d, REG%02X)", attempt,
                         item.reg);
                valid = false;
            }
        }
        for (const auto& item : kEs8311SuspendVerify) {
            uint8_t actual = 0;
            if (ReadEs8311Reg(item.reg, &actual) != ESP_OK || actual != item.value) {
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

    // Terminal codec step. Failures are logged but never abort the shutdown - the
    // pins and the panel still have to go down.
    void SuspendEs8311() {
        esp_err_t result = ESP_FAIL;
        for (int attempt = 1; attempt <= kEs8311SuspendAttempts; attempt++) {
            result = WriteEs8311SuspendSequence(attempt);
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
