#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "boards/common/adc_battery_monitor.h"
#include "power_save_timer.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif
#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
#endif

#define TAG "DogePet"

class DogePet : public WifiBoard {
private:
    Button boot_button_;
    Button btn_a_;
    Button btn_b_;
    Button btn_c_;
    bool conversation_active_ = false;
    LcdDisplay* display_ = nullptr;
    AdcBatteryMonitor* adc_batt_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    // IMU removed to save space

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
    // Honor per-panel invert setting when provided, fallback to true for typical ST7789 1.54" panels
#ifdef DISPLAY_INVERT_COLOR
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
#else
    esp_lcd_panel_invert_color(panel, true);
#endif
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        // Boot button: long press enters Wi-Fi config; short press just wakes
        boot_button_.OnClick([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
        });
        boot_button_.OnLongPress([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            app.SetDeviceState(kDeviceStateWifiConfiguring);
            ResetWifiConfiguration();
        });

        btn_a_.OnClick([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            int vol = std::min(100, codec->output_volume() + 10);
            codec->SetOutputVolume(vol);
            if (display_) display_->ShowNotification(std::string("VOL ") + std::to_string(vol/10));
        });
        btn_a_.OnLongPress([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            if (display_) display_->ShowNotification("MAX VOL");
        });

        btn_b_.OnClick([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            int vol = std::max(0, codec->output_volume() - 10);
            codec->SetOutputVolume(vol);
            if (display_) display_->ShowNotification(std::string("VOL ") + std::to_string(vol/10));
        });
        btn_b_.OnLongPress([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            if (display_) display_->ShowNotification("MUTED");
        });

        // Button C: Toggle conversation mode (click to start/stop)
        btn_c_.OnClick([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            if (!conversation_active_) {
                Application::GetInstance().StartListening();
                conversation_active_ = true;
                if (display_) display_->ShowNotification("AI ON");
            } else {
                Application::GetInstance().StopListening();
                conversation_active_ = false;
                if (display_) display_->ShowNotification("AI OFF");
            }
        });
    }

    void InitializeBattery() {
        // Create only if a valid ADC channel is defined; no charge-detect pin
        adc_batt_ = new AdcBatteryMonitor(VBAT_ADC_UNIT, VBAT_ADC_CH, VBAT_UPPER_R, VBAT_LOWER_R);
    }

    // IMU functions removed

public:
    DogePet() :
        boot_button_(BOOT_BUTTON_GPIO),
        btn_a_(BUTTON_A_GPIO),
        btn_b_(BUTTON_B_GPIO),
        btn_c_(BUTTON_C_GPIO) {
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeBattery();
        // Idle power save: screen dims/sleeps when idle; restore on activity
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            // Friendly goodbye on sleep
            if (display_) display_->ShowNotification("BYE");
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
            if (auto bl = GetBacklight()) bl->SetBrightness(1);
            if (auto d = GetDisplay()) d->SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            if (auto d = GetDisplay()) d->SetPowerSaveMode(false);
            if (auto bl = GetBacklight()) bl->RestoreBrightness();
        });
        power_save_timer_->SetEnabled(true);
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }

        // IMU-related MCP tools removed to save space
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!adc_batt_) return false;
        charging = adc_batt_->IsCharging();
        discharging = adc_batt_->IsDischarging();
        level = adc_batt_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(DogePet);
