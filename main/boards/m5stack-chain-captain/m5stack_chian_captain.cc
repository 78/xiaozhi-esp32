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
#include <wifi_manager.h>
#include "M5IOE1.h"
#include "M5PM1.h"
#include "esp_timer.h"

#define TAG "M5StackChainCaptainBoard"



typedef enum {
    BSP_PA_MODE_1 = 1,
    BSP_PA_MODE_2 = 2,
    BSP_PA_MODE_3 = 3,
    BSP_PA_MODE_4 = 4,
} bsp_pa_mode_t;

class M5StackChainCaptainBoard : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1 pmic_;
    M5IOE1 ioe_;
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
        gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
        for (int i = 0; i < mode; i++) {
            gpio_set_level(AUDIO_CODEC_GPIO_PA, 1);
            esp_rom_delay_us(5);
            gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
            esp_rom_delay_us(5);
        }
        gpio_set_level(AUDIO_CODEC_GPIO_PA, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "PA mode %d set successfully", mode);
        return ESP_OK;
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        if (ioe_.begin(i2c_bus_, 0x4F, M5IOE1_I2C_FREQ_100K, M5IOE1_INT_MODE_POLLING) != M5IOE1_OK) {
            ESP_LOGE(TAG, "M5IOE1 begin failed");
            return;
        }

        if (pmic_.begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) {
            ESP_LOGE(TAG, "M5PM1 begin failed");
            return;
        }
        // PORT.B power enable
        pmic_.gpioSetMode(PMIC_PIN_BOOST_ENABLE, M5PM1_GPIO_MODE_OUTPUT);
        pmic_.digitalWrite(PMIC_PIN_BOOST_ENABLE, HIGH);
        // PORT.A power enable
        pmic_.setBoostEnable(true);
        // charge enable
        pmic_.setChargeEnable(true);
        // charge state
        pmic_.gpioSetMode(PMIC_PIN_CHARGE_STATE, M5PM1_GPIO_MODE_INPUT);

        // LCD power enable
        ioe_.pinMode(IOE_PIN_LCD_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_POWER, HIGH);
        // LCD reset
        ioe_.pinMode(IOE_PIN_LCD_RESET, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_RESET, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_RESET, HIGH);
        vTaskDelay(pdMS_TO_TICKS(20));
        // LCD backlight
        ioe_.pinMode(IOE_PIN_BACKLIGHT, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_BACKLIGHT, M5IOE1_DRIVE_PUSHPULL);
        ioe_.setPwmFrequency(1000);
        ioe_.setPwmDuty(M5IOE1_PWM_CH3, 100, false, true);

        // CODEC power enable
        ioe_.pinMode(IOE_PIN_CODEC_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_CODEC_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_CODEC_POWER, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
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
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_set_gap(panel, 0, 80);
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

        volume_up_button_.OnClick([this]() {
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) volume = 100;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        });

        volume_down_button_.OnClick([this]() {
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() - 10;
            if (volume < 0) volume = 0;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        });
    }

public:
    M5StackChainCaptainBoard()
        : boot_button_(USER_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
          display_(nullptr),
          i2c_bus_(nullptr) {
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        bsp_audio_set_pa_mode(BSP_PA_MODE_4);
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

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        uint16_t voltage_mv = 0;
        if (pmic_.readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }
        charging = (pmic_.digitalRead(PMIC_PIN_CHARGE_STATE) == 0);
        discharging = !charging;
        const int BATTERY_MIN_VOLTAGE = 3000;
        const int BATTERY_MAX_VOLTAGE = 4200;
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
