#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "assets/lang_config.h"
#include "backlight.h"
#include "settings.h"
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <esp_log.h>
#include <wifi_manager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "M5PM1.h"

#define TAG "M5StackSticks3"

// reduce the output volume to 60%
class Sticks3AudioCodec : public Es8311AudioCodec {
public:
    Sticks3AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true, bool pa_inverted = false)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                          mclk, bclk, ws, dout, din, pa_pin, es8311_addr, use_mclk, pa_inverted) {}

    virtual void SetOutputVolume(int volume) override {
        if (volume > 100) {
            volume = 100;
        }
        if (volume < 0) {
            volume = 0;
        }

        // Scale volume to 60% (x 0.6)
        int scaled_volume = (int)(volume * 0.6f + 0.5f); // Round to nearest integer
        ESP_LOGI(TAG, "Requested output volume: %d%%, scaled to hardware: %d%%", volume, scaled_volume);

        // Call parent's SetOutputVolume with scaled value for hardware
        Es8311AudioCodec::SetOutputVolume(scaled_volume);
        
        // Update output_volume_ to original value for display
        output_volume_ = volume;
        
        // Save original value to settings
        Settings settings("audio", true);
        settings.SetInt("output_volume", volume);
    }
};

class M5StackSticks3Board : public WifiBoard {
private:
    Button boot_button_;
    Button user_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1* pmic_;

    void InitializeI2c() {
        // Initialize I2C peripheral (SYS_I2C/I2C0)
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializePm1() {
        // Initialize PM1 device
        ESP_LOGI(TAG, "M5Stack PMIC Init.");
        pmic_ = new M5PM1();
        pmic_->begin(i2c_bus_, M5PM1_DEFAULT_ADDR);
        pmic_->setChargeEnable(true);
        pmic_->setBoostEnable(false);
        // Configure PM1 G0 as input for charging status detection
        // PM1 G0: low = charging, high = not charging
        pmic_->pinMode(0, INPUT);
        // Configure PM1 G2 (LCD/Audio Power) as output high
        pmic_->pinMode(2, OUTPUT);
        pmic_->gpioSetDrive(M5PM1_GPIO_NUM_2, M5PM1_GPIO_DRIVE_PUSHPULL);
        pmic_->digitalWrite(2, HIGH);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Initialize LCD Display");
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // Install panel IO
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // Install LCD driver
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_set_gap(panel, 0, 0);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeBacklight() {
        ESP_LOGI(TAG, "Initialize Backlight");
        // Backlight is initialized via PwmBacklight in GetBacklight()
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });

        // User button on GPIO12: single click sets stored volume to 70%
        user_button_.OnClick([this]() {
            AudioCodec* codec = GetAudioCodec();
            if (codec) {
                // Use base AudioCodec::SetOutputVolume to update stored value
                // without touching hardware that may not be initialized yet.
                codec->SetOutputVolume(60);
                codec->AudioCodec::SetOutputVolume(60);
                ESP_LOGI(TAG, "User button pressed: stored volume set to 80%%");
            } else {
                ESP_LOGW(TAG, "User button pressed but codec is not available yet");
            }
        });
    }

    void EnablePa() {
        if (pmic_) {
            ESP_LOGI(TAG, "Enable PA (PM1_G3)");
            pmic_->digitalWrite(3, HIGH);
        }
    }

    void DisablePa() {
        if (pmic_) {
            ESP_LOGI(TAG, "Disable PA (PM1_G3)");
            pmic_->digitalWrite(3, LOW);
        }
    }

public:
    M5StackSticks3Board() :
        boot_button_(BOOT_BUTTON_GPIO),
        user_button_(USER_BUTTON_GPIO),
        display_(nullptr),
        i2c_bus_(nullptr),
        pmic_(nullptr) {
        InitializeI2c();
        InitializePm1();  // Initialize PM1 after I2C, before LCD/Audio
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeBacklight();
        InitializeButtons();
        EnablePa(); // Enable PA after initialization
        GetBacklight()->SetBrightness(60);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Sticks3AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, // Not used, PA controlled by PM1_G3
            AUDIO_CODEC_ES8311_ADDR,
            false);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!pmic_) {
            return false;
        }

        // Get battery voltage in mV
        uint16_t voltage_mv = 0;
        if (pmic_->readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        // Get charging status from PM1_G0 (low = charging, high = not charging)
        int pm1_g0_level = pmic_->digitalRead(0);
        if (pm1_g0_level < 0) {
            return false;
        }
        charging = (pm1_g0_level == 0);  // Low level means charging
        discharging = !charging;

        // Convert voltage to battery level percentage
        // Typical Li-ion battery: 3.0V (0%) to 4.2V (100%)
        const int BATTERY_MIN_VOLTAGE = 3000;  // 3.0V
        const int BATTERY_MAX_VOLTAGE = 4200;  // 4.2V

        if (voltage_mv < BATTERY_MIN_VOLTAGE) {
            level = 0;
        } else if (voltage_mv > BATTERY_MAX_VOLTAGE) {
            level = 100;
        } else {
            level = ((voltage_mv - BATTERY_MIN_VOLTAGE) * 100) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
        }

        ESP_LOGD(TAG, "Battery: %d%% (%dmV), Charging: %s", level, voltage_mv, charging ? "Yes" : "No");
        return true;
    }
};

DECLARE_BOARD(M5StackSticks3Board);

