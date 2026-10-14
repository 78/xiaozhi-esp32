#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <cmath>
#include <string>

#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "mcp_server.h"
#include "nm_epd_display.h"
#include "wifi_board.h"

#define TAG "NmEpd420Board"

// RockBase NM-EPD-420-BW: ESP32-S3 + 4.2" 400x300 B/W e-paper (GYE042A87,
// SSD1683), ES8311 codec with LMD4737 PDM mic on its DMIC input, AHT20
// temperature/humidity sensor, and per-module power gates.
class NmEpd420Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    Button user_button_;
    NmEpdDisplay* display_ = nullptr;

    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t adc_cali_ = nullptr;

    // Each peripheral block has its own enable pin; drive them once at boot.
    void InitializePowerGates() {
        gpio_config_t outputs = {};
        outputs.intr_type = GPIO_INTR_DISABLE;
        outputs.mode = GPIO_MODE_OUTPUT;
        outputs.pin_bit_mask = (1ULL << CODEC_EN_GPIO) | (1ULL << LORA_EN_GPIO) |
                               (1ULL << TEMP_CTL_GPIO) | (1ULL << ADC_EN_GPIO);
        outputs.pull_down_en = GPIO_PULLDOWN_DISABLE;
        outputs.pull_up_en = GPIO_PULLUP_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&outputs));

        // The codec rail must be up before any ES8311 I2C transaction.
        gpio_set_level(CODEC_EN_GPIO, 1);
        gpio_hold_en(CODEC_EN_GPIO);
        // LoRa modem is unused by this firmware: keep it powered down.
        gpio_set_level(LORA_EN_GPIO, 0);
        gpio_hold_en(LORA_EN_GPIO);
        // Sensor and battery-ADC gates are only raised around a measurement.
        gpio_set_level(TEMP_CTL_GPIO, 0);
        gpio_set_level(ADC_EN_GPIO, 0);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            // During startup (before connected), pressing BOOT enters Wi-Fi
            // config mode without reboot.
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        user_button_.OnClick([this]() { Application::GetInstance().ToggleChatState(); });
        user_button_.OnLongPress([this]() { EnterWifiConfigMode(); });
    }

    void InitializeDisplay() {
        NmEpdPins pins = {};
        pins.cs = EPD_CS_PIN;
        pins.dc = EPD_DC_PIN;
        pins.rst = EPD_RST_PIN;
        pins.busy = EPD_BUSY_PIN;
        pins.mosi = EPD_MOSI_PIN;
        pins.sck = EPD_SCK_PIN;
        pins.spi_host = EPD_SPI_NUM;
        display_ = new NmEpdDisplay(DISPLAY_WIDTH, DISPLAY_HEIGHT, pins);
    }

    void InitializeBatteryAdc() {
        adc_oneshot_unit_init_cfg_t unit_cfg = {};
        unit_cfg.unit_id = ADC_UNIT_1;
        if (adc_oneshot_new_unit(&unit_cfg, &adc_handle_) != ESP_OK) {
            adc_handle_ = nullptr;
            ESP_LOGW(TAG, "battery ADC unit init failed");
            return;
        }
        adc_oneshot_chan_cfg_t chan_cfg = {};
        chan_cfg.atten = ADC_ATTEN_DB_12;
        chan_cfg.bitwidth = ADC_BITWIDTH_12;
        if (adc_oneshot_config_channel(adc_handle_, BATTERY_ADC_CHANNEL, &chan_cfg) != ESP_OK) {
            ESP_LOGW(TAG, "battery ADC channel config failed");
            return;
        }
        adc_cali_curve_fitting_config_t cali_cfg = {};
        cali_cfg.unit_id = ADC_UNIT_1;
        cali_cfg.atten = ADC_ATTEN_DB_12;
        cali_cfg.bitwidth = ADC_BITWIDTH_12;
        if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &adc_cali_) != ESP_OK) {
            adc_cali_ = nullptr;
            ESP_LOGW(TAG, "battery ADC calibration unavailable");
        }
    }

    int ReadBatteryVoltageMv() {
        if (adc_handle_ == nullptr) {
            return 0;
        }
        gpio_set_level(ADC_EN_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(5));

        int sum_mv = 0;
        int samples = 0;
        for (int i = 0; i < 10; i++) {
            int raw = 0;
            if (adc_oneshot_read(adc_handle_, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
                continue;
            }
            int pin_mv = 0;
            if (adc_cali_ != nullptr &&
                adc_cali_raw_to_voltage(adc_cali_, raw, &pin_mv) == ESP_OK) {
                sum_mv += pin_mv;
            } else {
                sum_mv += raw * 3300 / 4095;  // rough fallback without calibration
            }
            samples++;
        }
        gpio_set_level(ADC_EN_GPIO, 0);
        if (samples == 0) {
            return 0;
        }
        return sum_mv / samples * BATTERY_ADC_DIV;
    }

    bool ReadAht20(float* temperature, float* humidity) {
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = AHT20_I2C_ADDR;
        dev_cfg.scl_speed_hz = 100000;
        i2c_master_dev_handle_t dev = nullptr;
        if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &dev) != ESP_OK) {
            return false;
        }

        gpio_set_level(TEMP_CTL_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(50));  // sensor power-up

        const uint8_t init_cmd[] = {0xBE, 0x08, 0x00};
        esp_err_t err = i2c_master_transmit(dev, init_cmd, sizeof(init_cmd), 100);
        vTaskDelay(pdMS_TO_TICKS(10));
        const uint8_t trigger[] = {0xAC, 0x33, 0x00};
        if (err == ESP_OK) {
            err = i2c_master_transmit(dev, trigger, sizeof(trigger), 100);
        }
        vTaskDelay(pdMS_TO_TICKS(80));  // measurement time

        uint8_t data[6] = {};
        if (err == ESP_OK) {
            err = i2c_master_receive(dev, data, sizeof(data), 100);
        }

        gpio_set_level(TEMP_CTL_GPIO, 0);
        i2c_master_bus_rm_device(dev);

        if (err != ESP_OK || (data[0] & 0x80)) {  // busy bit still set
            return false;
        }
        uint32_t raw_h = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
        uint32_t raw_t = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
        *humidity = raw_h * 100.0f / 1048576.0f;
        *temperature = raw_t * 200.0f / 1048576.0f - 50.0f;
        return true;
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.disp.network", "重新配网", PropertyList(),
                           [this](const PropertyList&) -> ReturnValue {
                               EnterWifiConfigMode();
                               return true;
                           });
        mcp_server.AddTool(
            "self.sensor.get_temperature_humidity", "获取板载 AHT20 传感器的当前温度和湿度",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                float temperature = 0, humidity = 0;
                if (!ReadAht20(&temperature, &humidity)) {
                    return std::string("传感器读取失败");
                }
                char buf[64];
                snprintf(buf, sizeof(buf), "温度: %.1f°C, 湿度: %.0f%%", temperature, humidity);
                return std::string(buf);
            });
    }

public:
    NmEpd420Board() : boot_button_(BOOT_BUTTON_GPIO), user_button_(USER_BUTTON_GPIO) {
        InitializePowerGates();
        InitializeI2c();
        InitializeBatteryAdc();
        InitializeButtons();
        InitializeDisplay();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true /* use_mclk */,
            false /* pa_inverted */, AUDIO_CODEC_USE_DMIC);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        int voltage_mv = ReadBatteryVoltageMv();
        if (voltage_mv <= 0) {
            return false;
        }
        // No charge-status pins on this board; report as discharging.
        charging = false;
        discharging = true;
        // Li-Po discharge curve approximation shared with the vendor demo.
        int percent = (-voltage_mv * voltage_mv + 9016 * voltage_mv - 19189000) / 10000;
        level = percent > 100 ? 100 : (percent < 0 ? 0 : percent);
        return true;
    }
};

DECLARE_BOARD(NmEpd420Board);
