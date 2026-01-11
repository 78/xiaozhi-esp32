#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "assets/lang_config.h"
#include "backlight.h"
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_manager.h>
#include "m5stack_ioe1.h"
#include "m5stack_pm1.h"
#include "esp_timer.h"


#define TAG "M5StackChainCaptainBoard"


typedef enum {
    BSP_PA_MODE_1 = 1, /*!< PA Mode 1 - 1 rising edge pulse */
    BSP_PA_MODE_2 = 2, /*!< PA Mode 2 - 2 rising edge pulses (1W speaker) */
    BSP_PA_MODE_3 = 3, /*!< PA Mode 3 - 3 rising edge pulses */
    BSP_PA_MODE_4 = 4, /*!< PA Mode 4 - 4 rising edge pulses */
} bsp_pa_mode_t;

class M5IoE1Backlight : public Backlight {
public:
    M5IoE1Backlight(m5ioe1_handle_t ioe) : Backlight(), ioe_(ioe) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        // m5ioe1_pwm_set_duty expects duty cycle 0-100
        // brightness is already 0-100, so we can use it directly
        m5ioe1_pwm_set_duty(ioe_, M5IOE1_PWM_CH3, brightness);
        brightness_ = brightness;
    }

private:
    m5ioe1_handle_t ioe_;
};

class M5StackChainCaptainBoard : public WifiBoard {
private:
    
    Button boot_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    m5pm1_handle_t pmic_;
    m5ioe1_handle_t ioe_;
    M5IoE1Backlight* backlight_;
    bool pa_pin_configured = false;

    esp_err_t bsp_audio_set_pa_mode(bsp_pa_mode_t mode) {
        if (!pa_pin_configured) {
            gpio_config_t pa_io_conf = {
                .pin_bit_mask = (1ULL << AUDIO_CODEC_GPIO_PA),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&pa_io_conf);
            gpio_set_level(AUDIO_CODEC_GPIO_PA, 1);
            pa_pin_configured = true;
        }

        if (mode < BSP_PA_MODE_1 || mode > BSP_PA_MODE_4) {
            ESP_LOGE(TAG, "Invalid PA mode: %d", mode);
            return ESP_ERR_INVALID_ARG;
        }
        
        if (!pa_pin_configured) {
            ESP_LOGE(TAG, "PA pin not configured");
            return ESP_ERR_INVALID_STATE;
        }
        
        ESP_LOGI(TAG, "Setting PA mode to %d", mode);
        
        // Step 1: Keep PA pin low for more than 1ms
        gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
        vTaskDelay(pdMS_TO_TICKS(2)); // 2ms delay to ensure >1ms
        
        // Step 2: Send the required number of rising edge pulses
        // Each pulse: 0.75us < TL, TH < 10us
        for (int i = 0; i < mode; i++) {
            // Rising edge: Low -> High -> Low
            gpio_set_level(AUDIO_CODEC_GPIO_PA, 1);
            esp_rom_delay_us(5); // 5us high time (within 0.75us < TH < 10us)
            gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
            esp_rom_delay_us(5); // 5us low time (within 0.75us < TL < 10us)
        }

        // Final state: keep PA pin low
        gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); 
        
        ESP_LOGI(TAG, "PA mode %d set successfully", mode);
        return ESP_OK;
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
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

        I2cDetect();

        ioe_ = m5ioe1_create(i2c_bus_, M5IOE1_I2C_ADDRESS_DEFAULT);

        ESP_LOGI(TAG, "M5Stack PMIC Init.");
        pmic_ = m5pm1_create(i2c_bus_, M5PM1_I2C_ADDRESS_DEFAULT, GPIO_NUM_NC);
        ESP_LOGI(TAG, "PMIC Verion: %d.%d", m5pm1_get_hw_revision(pmic_) >> 4, m5pm1_get_hw_revision(pmic_) & 0x0F);

        vTaskDelay(pdMS_TO_TICKS(500));

        m5pm1_set_5v_boost(pmic_, false); // ???

        vTaskDelay(pdMS_TO_TICKS(500));

        // LCD Power enable
        m5ioe1_pin_mode(ioe_, 12, true);
        m5ioe1_set_drive_mode(ioe_, 12, false); // 推挽输出
        m5ioe1_digital_write(ioe_, 12, true);

        // LCD Reset
        m5ioe1_pin_mode(ioe_, 1, true);
        m5ioe1_set_drive_mode(ioe_, 1, false); // 推挽输出
        // m5ioe1_digital_write(ioe_, 1, false);
        // vTaskDelay(pdMS_TO_TICKS(100));
        m5ioe1_digital_write(ioe_, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));

        // LCD Backlight enable
        m5ioe1_pin_mode(ioe_, 11, true);
        m5ioe1_set_drive_mode(ioe_, 11, false); // 推挽输出
        m5ioe1_pwm_set_frequency(ioe_, 1000);
        m5ioe1_pwm_config(ioe_, M5IOE1_PWM_CH3, 0, M5IOE1_PWM_POLARITY_HIGH, true);
        m5ioe1_pwm_set_duty(ioe_, M5IOE1_PWM_CH3, 80);

        // CODEC Power enable
        m5ioe1_pin_mode(ioe_, 5, true);
        m5ioe1_set_drive_mode(ioe_, 5, false);  // 推挽输出
        m5ioe1_digital_write(ioe_, 5, true);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    void InitializeSpi() {
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
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
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
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_set_gap(panel, 0, 80); // 
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });
    }

public:
    M5StackChainCaptainBoard() :
        boot_button_(GPIO_NUM_1),
        pmic_(nullptr),
        ioe_(nullptr),
        backlight_(nullptr) {
        InitializeI2c();
        // I2cDetect();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        bsp_audio_set_pa_mode(BSP_PA_MODE_4);
        
        // Initialize backlight
        backlight_ = new M5IoE1Backlight(ioe_);
        backlight_->RestoreBrightness();
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
            AUDIO_CODEC_GPIO_PA, 
            AUDIO_CODEC_ES8311_ADDR, 
            false);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!pmic_) {
            return false;
        }

        // Get battery voltage in mV
        uint16_t voltage_mv = m5pm1_get_battery_voltage(pmic_);

        // Get power source status to determine charging state
        bool battery_valid = false;
        bool vinout_5v_valid = false;
        bool vin_5v_valid = false;
        m5pm1_get_power_source_status(pmic_, &battery_valid, &vinout_5v_valid, &vin_5v_valid);

        // If 5V input is valid, device is charging
        charging = vin_5v_valid || vinout_5v_valid;
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

DECLARE_BOARD(M5StackChainCaptainBoard);
