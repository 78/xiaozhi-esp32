#include <stdio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "wifi_board.h"
#include "board_power_bsp.h"
#include "custom_lcd_display.h"
#include "lvgl.h"
#include "mcp_server.h"

#define TAG "waveshare_epaper_1_54"

class CustomBoard : public WifiBoard {
  private:
    i2c_master_bus_handle_t   i2c_bus_;
    Button                    boot_button_;
    Button                    pwr_button_;
    CustomLcdDisplay         *display_;
    BoardPowerBsp            *power_;
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t         cali_handle;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port          = (i2c_port_t) 0;
        i2c_bus_cfg.sda_io_num        = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num        = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source        = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority     = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        pwr_button_.OnLongPress([this]() {
            GetDisplay()->SetChatMessage("system", "OFF");
            vTaskDelay(pdMS_TO_TICKS(1000));
            power_->PowerAudioOff();
            power_->PowerEpdOff();
            power_->VbatPowerOff();
        });
    }

    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.disp.network", "重新配网", PropertyList(), [this](const PropertyList &) -> ReturnValue {
            EnterWifiConfigMode();
            return true;
        });
    }

    void InitializeLcdDisplay() {
        custom_lcd_spi_t lcd_spi_data = {};
        lcd_spi_data.cs               = EPD_CS_PIN;
        lcd_spi_data.dc               = EPD_DC_PIN;
        lcd_spi_data.rst              = EPD_RST_PIN;
        lcd_spi_data.busy             = EPD_BUSY_PIN;
        lcd_spi_data.mosi             = EPD_MOSI_PIN;
        lcd_spi_data.scl              = EPD_SCK_PIN;
        lcd_spi_data.spi_host         = EPD_SPI_NUM;
        lcd_spi_data.buffer_len       = 5000;
        display_                      = new CustomLcdDisplay(NULL, NULL, EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, lcd_spi_data);
    }

    void Power_Init() {
        power_ = new BoardPowerBsp(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);
        power_->VbatPowerOn();
        power_->PowerAudioOn();
        power_->PowerEpdOn();
        do {
            vTaskDelay(pdMS_TO_TICKS(10));
        } while (!gpio_get_level(VBAT_PWR_GPIO));
    }

    uint16_t BatterygetVoltage(void) {
        static bool initialized = false;
        static adc_oneshot_unit_handle_t adc_handle;
        static adc_cali_handle_t cali_handle = NULL;
        if (!initialized) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
            };
            adc_oneshot_new_unit(&init_config, &adc_handle);
    
            adc_oneshot_chan_cfg_t ch_config = {
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &ch_config);
    
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
                initialized = true;
            }
        }

        if (initialized) {
            int raw_value = 0;
            int raw_voltage = 0;
            int voltage = 0; // mV
            adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &raw_value);
            adc_cali_raw_to_voltage(cali_handle, raw_value, &raw_voltage);
            voltage =  raw_voltage * 2;
            // ESP_LOGI(TAG, "voltage: %dmV", voltage);
            return (uint16_t)voltage;
        }

        return 0;
    }

    uint8_t BatterygetPercent() {
        int voltage = 0;
        for (uint8_t i = 0; i < 10; i++) {
            voltage += BatterygetVoltage();
        }

        voltage /= 10;
        int percent = (-1 * voltage * voltage + 9016 * voltage - 19189000) / 10000;
        percent = (percent > 100) ? 100 : (percent < 0) ? 0 : percent;
        // ESP_LOGI(TAG, "voltage: %dmV, percentage: %d%%", voltage, percent);
        return (uint8_t)percent;
    }

  public:
    CustomBoard() : boot_button_(BOOT_BUTTON_GPIO), pwr_button_(VBAT_PWR_GPIO) {
        Power_Init();
        InitializeI2c();
        InitializeButtons();
        InitializeTools();
        InitializeLcdDisplay();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = false;
        discharging = !charging;
        level = (int)BatterygetPercent();

        return true;
    }
};

DECLARE_BOARD(CustomBoard);