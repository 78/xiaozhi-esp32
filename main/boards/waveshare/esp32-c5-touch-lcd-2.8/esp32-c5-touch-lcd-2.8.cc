#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "custom_io_expander_ch32v003.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_touch_cst3530.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr char kTag[] = "WaveshareC5TouchLcd28";

class ExpanderBacklight : public Backlight {
public:
    explicit ExpanderBacklight(esp_io_expander_handle_t expander) : expander_(expander) {}

protected:
    void SetBrightnessImpl(uint8_t brightness) override {
        // This board uses non-inverted, 8-bit CH32V003 PWM, as in its factory BSP.
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            custom_io_expander_set_pwm(expander_, brightness * 255 / 100));
    }

private:
    esp_io_expander_handle_t expander_;
};
}  // namespace

class WaveshareEsp32C5TouchLcd28 : public WifiBoard {
private:
    Button boot_button_{BOOT_BUTTON_GPIO};
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_io_expander_handle_t io_expander_ = nullptr;
    LcdDisplay* display_ = nullptr;
    ExpanderBacklight* backlight_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t config = {};
        config.i2c_port = I2C_NUM_0;
        config.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        config.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7;
        config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &i2c_bus_));
    }

    void InitializeIoExpander() {
        ESP_ERROR_CHECK(custom_io_expander_new_i2c_ch32v003(
            i2c_bus_, CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS, &io_expander_));
        ESP_ERROR_CHECK(esp_io_expander_set_dir(
            io_expander_, IO_EXPANDER_TOUCH_RESET | IO_EXPANDER_LCD_RESET | IO_EXPANDER_PA_ENABLE,
            IO_EXPANDER_OUTPUT));
        ESP_ERROR_CHECK(esp_io_expander_set_level(
            io_expander_, IO_EXPANDER_TOUCH_RESET | IO_EXPANDER_LCD_RESET | IO_EXPANDER_PA_ENABLE,
            1));
        ESP_ERROR_CHECK(custom_io_expander_set_pwm(io_expander_, 0));
        backlight_ = new ExpanderBacklight(io_expander_);
        ESP_LOGI(kTag, "CH32V003 initialized; speaker amplifier enabled on EXIO3");
    }

    void InitializeSpi() {
        // LCD and SD share SPI. Keep an inserted SD card deselected.
        gpio_config_t sd_cs = {};
        sd_cs.pin_bit_mask = 1ULL << SDCARD_CS_PIN;
        sd_cs.mode = GPIO_MODE_OUTPUT;
        ESP_ERROR_CHECK(gpio_config(&sd_cs));
        ESP_ERROR_CHECK(gpio_set_level(SDCARD_CS_PIN, 1));

        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DISPLAY_MOSI_PIN;
        bus_config.miso_io_num = DISPLAY_MISO_PIN;
        bus_config.sclk_io_num = DISPLAY_CLK_PIN;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_LCD_RESET, 0));
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_LCD_RESET, 1));
        vTaskDelay(pdMS_TO_TICKS(200));

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_TOUCH_RESET, 0));
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_TOUCH_RESET, 1));
        vTaskDelay(pdMS_TO_TICKS(10));

        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST3530_ADDRESS;
        io_config.scl_speed_hz = 100000;
        io_config.control_phase_bytes = 1;
        io_config.lcd_cmd_bits = 8;
        io_config.flags.disable_control_phase = true;
        esp_lcd_panel_io_handle_t touch_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &touch_io));

        esp_lcd_touch_config_t touch_config = {};
        touch_config.x_max = DISPLAY_WIDTH;
        touch_config.y_max = DISPLAY_HEIGHT;
        touch_config.rst_gpio_num = GPIO_NUM_NC;
        touch_config.int_gpio_num = DISPLAY_TOUCH_INT_PIN;
        // CST3530 1.0.0 needs the bus here for its direct 32-bit I2C commands.
        touch_config.driver_data = i2c_bus_;
        esp_lcd_touch_handle_t touch = nullptr;
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst3530(touch_io, &touch_config, &touch));

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        lvgl_port_touch_cfg_t touch_port_config = {};
        touch_port_config.disp = lv_display_get_default();
        touch_port_config.handle = touch;
        auto indev = lvgl_port_add_touch(&touch_port_config);
        lvgl_port_unlock();
        ESP_ERROR_CHECK(indev != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
        ESP_LOGI(kTag, "CST3530 touch initialized");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    WaveshareEsp32C5TouchLcd28() {
        InitializeI2c();
        InitializeIoExpander();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        backlight_->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        // No physical MCLK connection: ES8311 derives its clock from BCLK.
        static Es8311AudioCodec codec(
            i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, false);
        return &codec;
    }

    Display* GetDisplay() override { return display_; }

    Backlight* GetBacklight() override { return backlight_; }
};

DECLARE_BOARD(WaveshareEsp32C5TouchLcd28);
