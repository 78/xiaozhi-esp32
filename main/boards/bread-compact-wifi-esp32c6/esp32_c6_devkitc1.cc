/**
 * @file esp32_c6_devkitc1.cc
 * @brief Placa ESP32-C6-DevKitC-1 para XiaoZhi AI
 *
 * Hardware:
 *   - ESP32-C6-DevKitC-1 (8MB Flash)
 *   - OLED SSD1306 128x64 por I2C (SDA=GPIO6, SCL=GPIO7)
 *   - Botón BOOT en GPIO9 (activo en bajo)
 */

#include "wifi_board.h"
#include "display/oled_display.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#define TAG "ESP32C6DevKitC1"

// Pines
#define I2C_SDA_PIN         GPIO_NUM_6
#define I2C_SCL_PIN         GPIO_NUM_7
#define BOOT_BUTTON_PIN     GPIO_NUM_9
#define DISPLAY_I2C_ADDR    0x3C
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64

// Fuentes LVGL
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class Esp32C6DevKitC1Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    NoAudioCodec audio_codec_;  // ← Agregar esta línea

    void InitializeI2c() {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
        ESP_LOGI(TAG, "I2C OK — SDA=GPIO%d SCL=GPIO%d", I2C_SDA_PIN, I2C_SCL_PIN);
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_i2c_config_t io_cfg = {
            .dev_addr = DISPLAY_I2C_ADDR,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = { .dc_low_on_data = 0, .disable_control_phase = 0 },
            .scl_speed_hz = 400 * 1000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_cfg, &panel_io_));

        esp_lcd_panel_dev_config_t panel_cfg = {};
        panel_cfg.reset_gpio_num = -1;
        panel_cfg.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_cfg = { .height = DISPLAY_HEIGHT };
        panel_cfg.vendor_config = &ssd1306_cfg;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_cfg, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        ESP_LOGI(TAG, "SSD1306 128x64 OK");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "BOOT presionado");
            auto& app = Application::GetInstance();
            app.WakeWordInvoke("你好小智");
        });
        boot_button_.OnLongPress([this]() {
            ESP_LOGW(TAG, "BOOT largo — reseteando WiFi");
            ResetWifiConfiguration();
        });
        ESP_LOGI(TAG, "Botón BOOT GPIO%d registrado", BOOT_BUTTON_PIN);
    }

public:
    Esp32C6DevKitC1Board() : boot_button_(BOOT_BUTTON_PIN, false) {
        ESP_LOGI(TAG, "=== ESP32-C6-DevKitC-1 ===");
        InitializeI2c();
        InitializeDisplay();
        InitializeButtons();
    }

    virtual Display* GetDisplay() override {
        if (display_ == nullptr) {
            DisplayFonts fonts = {
                .text_font = &font_puhui_14_1,
                .icon_font = &font_awesome_14_1,
            };
            display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                       false, false, fonts);
        }
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
      //  return &audio_codec_;
      static NoAudioCodec codec;
      return &codec;
    }
};

DECLARE_BOARD(Esp32C6DevKitC1Board)
