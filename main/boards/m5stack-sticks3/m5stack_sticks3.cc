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

#define TAG "M5StackSticks3"


// class Sticks3AudioCodec : public Es8311AudioCodec {
// public:
//     Sticks3AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
//         gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
//         gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true, bool pa_inverted = false)
//         : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
//                           mclk, bclk, ws, dout, din, pa_pin, es8311_addr, use_mclk, pa_inverted) {}

//     virtual void SetOutputVolume(int volume) override {

//         if (volume > 100) {
//             volume = 100;
//         }
//         if (volume < 0) {
//             volume = 0;
//         }

//         int scaled_volume = (int)(volume * 0.6f + 0.5f); // Round to nearest integer
//         ESP_LOGI(TAG, "Requested output volume: %d", volume);

//         // Call parent's SetOutputVolume with scaled value for hardware
//         // This will set the hardware volume and save to settings
//         Es8311AudioCodec::SetOutputVolume(scaled_volume); // Es8311 max volume is 60
        
//         // Update output_volume_ to original value for display
//         output_volume_ = volume;
        
//         // Save original value to settings
//         Settings settings("audio", true);
//         settings.SetInt("output_volume", volume);
//     }
// };

// PM1 register addresses
#define PM1_REG_GPIO_FUNC   0x16 // GPIO function register
#define PM1_REG_GPIO_MODE   0x10 // GPIO mode register (input=0, output=1)
#define PM1_REG_GPIO_DRIVE  0x13 // GPIO drive mode register (open-drain=1, push-pull=0)
#define PM1_REG_GPIO_OUTPUT 0x11 // GPIO output register
#define PM1_REG_GPIO_INPUT  0x12 // GPIO input register
#define PM1_REG_CHARGE_CTRL 0x06 // Charge control register
#define PM1_REG_BAT_L       0x22 // Battery voltage low byte
#define PM1_REG_BAT_H       0x23 // Battery voltage high byte
#define PM1_REG_I2C_CFG     0x09 // I2C configuration register

class Pm1Device : public I2cDevice {
public:
    Pm1Device(i2c_master_bus_handle_t i2c_bus, uint8_t addr) 
        : I2cDevice(i2c_bus, addr, 100 * 1000) {}

    // Set GPIO pin as GPIO function
    esp_err_t SetGpioFunction(uint8_t pin, bool enable) {
        uint8_t reg_val = ReadReg(PM1_REG_GPIO_FUNC);
        if (enable) {
            reg_val &= ~(1 << pin); // Clear bit to set as GPIO
        } else {
            reg_val |= (1 << pin);  // Set bit to set as other function
        }
        WriteReg(PM1_REG_GPIO_FUNC, reg_val);
        return ESP_OK;
    }

    // Set GPIO pin mode (input=0, output=1)
    esp_err_t SetGpioMode(uint8_t pin, bool output) {
        uint8_t reg_val = ReadReg(PM1_REG_GPIO_MODE);
        if (output) {
            reg_val |= (1 << pin);  // Set bit for output
        } else {
            reg_val &= ~(1 << pin); // Clear bit for input
        }
        WriteReg(PM1_REG_GPIO_MODE, reg_val);
        return ESP_OK;
    }

    // Set GPIO drive mode (open-drain=1, push-pull=0)
    esp_err_t SetGpioDriveMode(uint8_t pin, bool open_drain) {
        uint8_t reg_val = ReadReg(PM1_REG_GPIO_DRIVE);
        if (open_drain) {
            reg_val |= (1 << pin);  // Set bit for open-drain
        } else {
            reg_val &= ~(1 << pin); // Clear bit for push-pull
        }
        WriteReg(PM1_REG_GPIO_DRIVE, reg_val);
        return ESP_OK;
    }

    // Set GPIO output level
    esp_err_t SetGpioOutput(uint8_t pin, bool high) {
        uint8_t reg_val = ReadReg(PM1_REG_GPIO_OUTPUT);
        if (high) {
            reg_val |= (1 << pin);  // Set bit for high
        } else {
            reg_val &= ~(1 << pin); // Clear bit for low
        }
        WriteReg(PM1_REG_GPIO_OUTPUT, reg_val);
        return ESP_OK;
    }

    // Configure GPIO pin as output push-pull
    esp_err_t ConfigureGpioOutput(uint8_t pin, bool high) {
        ESP_LOGI(TAG, "Configuring PM1_G%d as output", pin);
        ESP_ERROR_CHECK(SetGpioFunction(pin, true));   // Set as GPIO function
        ESP_ERROR_CHECK(SetGpioMode(pin, true));       // Set as output
        ESP_ERROR_CHECK(SetGpioDriveMode(pin, false)); // Set as push-pull
        ESP_ERROR_CHECK(SetGpioOutput(pin, high));     // Set output level
        return ESP_OK;
    }

    // Configure GPIO pin as input
    esp_err_t ConfigureGpioInput(uint8_t pin) {
        ESP_LOGI(TAG, "Configuring PM1_G%d as input", pin);
        ESP_ERROR_CHECK(SetGpioFunction(pin, true));   // Set as GPIO function
        ESP_ERROR_CHECK(SetGpioMode(pin, false));      // Set as input
        return ESP_OK;
    }

    // Read GPIO input level
    bool ReadGpioInput(uint8_t pin) {
        uint8_t reg_val = ReadReg(PM1_REG_GPIO_INPUT);
        return (reg_val & (1 << pin)) != 0;
    }

    // Get charging status (PM1_G0: low=charging, high=not charging)
    bool IsCharging() {
        return !ReadGpioInput(0);  // Low = charging
    }

    // Control charge enable (register 0x06 bit 0: 1=enable, 0=disable)
    esp_err_t SetChargeEnable(bool enable) {
        uint8_t reg_val = ReadReg(PM1_REG_CHARGE_CTRL);
        if (enable) {
            reg_val |= 0x01;  // Set bit 0
        } else {
            reg_val &= ~0x01; // Clear bit 0
        }
        WriteReg(PM1_REG_CHARGE_CTRL, reg_val);
        return ESP_OK;
    }

    // Control 5V output (register 0x06 bit 3: 1=enable, 0=disable)
    esp_err_t Set5VOutput(bool enable) {
        uint8_t reg_val = ReadReg(PM1_REG_CHARGE_CTRL);
        if (enable) {
            reg_val |= 0x08;  // Set bit 3
        } else {
            reg_val &= ~0x08; // Clear bit 3
        }
        WriteReg(PM1_REG_CHARGE_CTRL, reg_val);
        return ESP_OK;
    }

    // Get battery voltage in mV
    int GetBatteryVoltage() {
        uint8_t buf[2];
        ReadRegs(PM1_REG_BAT_L, buf, 2);
        return (buf[1] << 8) | buf[0]; // Format: (BAT_H << 8) | BAT_L, unit: mV
    }

    // Write register (public method to access protected WriteReg)
    esp_err_t WriteRegister(uint8_t reg, uint8_t value) {
        WriteReg(reg, value);
        return ESP_OK;
    }

    // Read register (public method to access protected ReadReg)
    uint8_t ReadRegister(uint8_t reg) {
        return ReadReg(reg);
    }
};

class M5StackSticks3Board : public WifiBoard {
private:
    Button boot_button_;
    Button user_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    Pm1Device* pm1_;

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
        ESP_LOGI(TAG, "Initialize PM1 PMIC");
        pm1_ = new Pm1Device(i2c_bus_, PM1_I2C_ADDR);

        // Configure PM1_G0 as input for charging status
        ESP_LOGI(TAG, "Configure charging status input (PM1_G0)");
        pm1_->ConfigureGpioInput(0);

        // Configure PM1_G2 (LCD/Audio Power) as output high
        ESP_LOGI(TAG, "Enable LCD/Audio Power (PM1_G2)");
        pm1_->ConfigureGpioOutput(2, true);
        vTaskDelay(pdMS_TO_TICKS(20));

        // Configure PM1_G3 (PA Control) -- aw8737a max output power to 0.6W
        ESP_LOGI(TAG, "Initialize PA Control (PM1_G3)");
        pm1_->ConfigureGpioOutput(3, false);
        vTaskDelay(pdMS_TO_TICKS(2));
        pm1_->WriteRegister(0x53, 0xE3);
        // pm1_->SetGpioOutput(3, true);
        vTaskDelay(pdMS_TO_TICKS(3));
        // pm1_->SetGpioOutput(3, false);

        // Disable 5V output initially
        ESP_LOGI(TAG, "Disable 5V output");
        pm1_->Set5VOutput(false);
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
                codec->SetOutputVolume(80);
                codec->AudioCodec::SetOutputVolume(80);
                ESP_LOGI(TAG, "User button pressed: stored volume set to 80%%");
            } else {
                ESP_LOGW(TAG, "User button pressed but codec is not available yet");
            }
        });
    }

    void EnablePa() {
        if (pm1_) {
            ESP_LOGI(TAG, "Enable PA (PM1_G3)");
            pm1_->SetGpioOutput(3, true);
        }
    }

    void DisablePa() {
        if (pm1_) {
            ESP_LOGI(TAG, "Disable PA (PM1_G3)");
            pm1_->SetGpioOutput(3, false);
        }
    }

public:
    M5StackSticks3Board() :
        boot_button_(BOOT_BUTTON_GPIO),
        user_button_(USER_BUTTON_GPIO) {
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
        static Es8311AudioCodec audio_codec(
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
        if (!pm1_) {
            return false;
        }

        // Get charging status (PM1_G0: low=charging, high=not charging)
        charging = pm1_->IsCharging();
        discharging = !charging;

        // Get battery voltage in mV
        int voltage_mv = pm1_->GetBatteryVoltage();

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

