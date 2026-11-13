#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "custom_io_expander_ch32v003.h"
#include "display/lcd_display.h"
#include "esp_video.h"
#include "mcp_server.h"
#include "power_save_timer.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <XPowersAXP2101.hpp>

namespace {
constexpr char kTag[] = "WaveshareC5TouchLcd35";

struct LcdInitCommand {
    int command;
    const void* data;
    size_t size;
    uint32_t delay_ms;
};

// Preserve the panel-specific initialization from the factory BSP.
const LcdInitCommand kLcdInitCommands[] = {
    {0x11, nullptr, 0, 100},
    {0x36, "\x08", 1, 0},
    {0x3A, "\x55", 1, 0},
    {0x11, nullptr, 0, 120},
    {0x3A, "\x05", 1, 0},
    {0xF0, "\xC3", 1, 0},
    {0xF0, "\x96", 1, 0},
    {0xB4, "\x01", 1, 0},
    {0xB7, "\xC6", 1, 0},
    {0xC0, "\x80\x45", 2, 0},
    {0xC1, "\x13", 1, 0},
    {0xC2, "\xA7", 1, 0},
    {0xC5, "\x0A", 1, 0},
    {0xE8, "\x40\x8A\x00\x00\x29\x19\xA5\x33", 8, 0},
    {0xE0, "\xD0\x08\x0F\x06\x06\x33\x30\x33\x47\x17\x13\x13\x2B\x31", 14, 0},
    {0xE1, "\xD0\x0A\x11\x0B\x09\x07\x2F\x33\x47\x38\x15\x16\x2C\x32", 14, 0},
    {0xF0, "\x3C", 1, 0},
    {0xF0, "\x69", 1, 120},
    {0x21, nullptr, 0, 0},
    {0x29, nullptr, 0, 0},
};

class ExpanderBacklight : public Backlight {
public:
    explicit ExpanderBacklight(esp_io_expander_handle_t expander) : expander_(expander) {}

protected:
    void SetBrightnessImpl(uint8_t brightness) override {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            custom_io_expander_set_pwm(expander_, brightness * 255 / 100));
    }

private:
    esp_io_expander_handle_t expander_;
};
}  // namespace

class WaveshareEsp32C5TouchLcd35 : public WifiBoard {
private:
    Button boot_button_{BOOT_BUTTON_GPIO};
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_io_expander_handle_t io_expander_ = nullptr;
    XPowersAXP2101 pmic_;
    LcdDisplay* display_ = nullptr;
    ExpanderBacklight* backlight_ = nullptr;
    EspVideo* camera_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;

    void InitializePowerSaveTimer() {
        // Keep wake-word detection running while the display is dimmed.
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            Application::GetInstance().Schedule([this]() {
                display_->SetPowerSaveMode(true);
                backlight_->SetBrightness(20);
            });
        });
        power_save_timer_->OnExitSleepMode([this]() {
            Application::GetInstance().Schedule([this]() {
                display_->SetPowerSaveMode(false);
                backlight_->RestoreBrightness();
            });
        });
        power_save_timer_->OnShutdownRequest([this]() {
            Application::GetInstance().Schedule([this]() {
                if (Application::GetInstance().CanEnterSleepMode() && pmic_.isBatteryConnect() &&
                    pmic_.isDischarge()) {
                    pmic_.shutdown();
                }
            });
        });
        power_save_timer_->SetEnabled(pmic_.isBatteryConnect() && pmic_.isDischarge());
    }

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

    void InitializePower() {
        ESP_ERROR_CHECK(pmic_.begin(i2c_bus_, AXP2101_SLAVE_ADDRESS) ? ESP_OK : ESP_FAIL);
        pmic_.disableWatchdog();
        pmic_.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
        pmic_.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
        pmic_.setSysPowerDownVoltage(2600);
        pmic_.setDC1Voltage(3300);
        pmic_.setDC2Voltage(1000);
        pmic_.setDC3Voltage(3300);
        pmic_.setDC4Voltage(1000);
        pmic_.setDC5Voltage(3300);
        pmic_.setALDO1Voltage(3300);
        pmic_.setALDO2Voltage(3300);
        pmic_.setALDO3Voltage(3300);
        pmic_.setALDO4Voltage(1800);
        pmic_.setBLDO1Voltage(1500);
        pmic_.setBLDO2Voltage(2800);
        pmic_.setCPUSLDOVoltage(1000);
        pmic_.setDLDO1Voltage(3300);
        // The factory firmware retains DLDO2's existing voltage (max 1.4 V).
        pmic_.enableDC1();
        pmic_.enableDC2();
        pmic_.enableDC3();
        pmic_.enableDC4();
        pmic_.enableDC5();
        pmic_.enableALDO1();
        pmic_.enableALDO2();
        pmic_.enableALDO3();
        pmic_.enableALDO4();
        pmic_.enableBLDO1();
        pmic_.enableBLDO2();
        pmic_.enableCPUSLDO();
        pmic_.enableDLDO1();
        pmic_.enableDLDO2();
        // Keep DC1 in PWM mode, matching the factory low-brightness flicker fix.
        pmic_.settDC1WorkModeToPwm(true);
        pmic_.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        pmic_.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
        pmic_.disableTSPinMeasure();
        pmic_.enableBattDetection();
        pmic_.enableVbusVoltageMeasure();
        pmic_.enableBattVoltageMeasure();
        pmic_.enableSystemVoltageMeasure();
        pmic_.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
        pmic_.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        pmic_.clearIrqStatus();
        pmic_.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
        pmic_.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
        pmic_.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
        pmic_.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);
        pmic_.enableButtonBatteryCharge();
        pmic_.setButtonBatteryChargeVoltage(3300);
    }

    void InitializeIoExpander() {
        ESP_ERROR_CHECK(custom_io_expander_new_i2c_ch32v003(
            i2c_bus_, CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS, &io_expander_));
        ESP_ERROR_CHECK(custom_io_expander_set_pwm(io_expander_, 0));
        constexpr uint32_t reset_pins = IO_EXPANDER_TOUCH_RESET | IO_EXPANDER_LCD_RESET;
        ESP_ERROR_CHECK(esp_io_expander_set_dir(io_expander_, reset_pins | IO_EXPANDER_PA_ENABLE,
                                                IO_EXPANDER_OUTPUT));
        ESP_ERROR_CHECK(
            esp_io_expander_set_dir(io_expander_, IO_EXPANDER_POWER_BUTTON, IO_EXPANDER_INPUT));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_PA_ENABLE, 0));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 1));
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 0));
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 1));
        vTaskDelay(pdMS_TO_TICKS(100));
        backlight_ = new ExpanderBacklight(io_expander_);
    }

    void InitializeSpi() {
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
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        esp_lcd_panel_handle_t panel = nullptr;
        // ST7796 shares ST7789's 16-bit SPI draw, MADCTL and address-window commands.
        // Use those panel operations, but initialize ST7796 with its factory commands
        // instead of calling esp_lcd_panel_init(), which sends ST7789-only settings.
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        for (const auto& command : kLcdInitCommands) {
            ESP_ERROR_CHECK(
                esp_lcd_panel_io_tx_param(panel_io, command.command, command.data, command.size));
            if (command.delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(command.delay_ms));
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        // FT6336 uses the FT5x06 coordinate registers and the same I2C address.
        esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        io_config.scl_speed_hz = 400000;
        esp_lcd_panel_io_handle_t touch_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &touch_io));
        esp_lcd_touch_config_t touch_config = {};
        touch_config.x_max = DISPLAY_WIDTH;
        touch_config.y_max = DISPLAY_HEIGHT;
        touch_config.rst_gpio_num = GPIO_NUM_NC;
        touch_config.int_gpio_num = DISPLAY_TOUCH_INT_PIN;
        esp_lcd_touch_handle_t touch = nullptr;
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(touch_io, &touch_config, &touch));
        const uint8_t gesture_disabled = 0;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(touch_io, 0xD0, &gesture_disabled, 1));

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        lvgl_port_touch_cfg_t config = {};
        config.disp = lv_display_get_default();
        config.handle = touch;
        auto indev = lvgl_port_add_touch(&config);
        lvgl_port_unlock();
        ESP_ERROR_CHECK(indev != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
        ESP_LOGI(kTag, "FT6336 touch initialized");
    }

    void InitializeCamera() {
        esp_video_init_spi_config_t spi = {};
        spi.sccb_config.init_sccb = false;
        spi.sccb_config.i2c_handle = i2c_bus_;
        spi.sccb_config.freq = 100000;
        spi.intf = ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO;
        spi.io_mode = ESP_CAM_CTLR_SPI_CAM_IO_MODE_2BIT;
        spi.spi_port = 1;
        spi.spi_cs_pin = CAMERA_CS_PIN;
        spi.spi_sclk_pin = CAMERA_SCLK_PIN;
        spi.spi_data0_io_pin = CAMERA_DATA0_PIN;
        spi.spi_data1_io_pin = CAMERA_DATA1_PIN;
        spi.spi_data2_io_pin = GPIO_NUM_NC;
        spi.spi_data3_io_pin = GPIO_NUM_NC;
        spi.reset_pin = GPIO_NUM_NC;
        spi.pwdn_pin = GPIO_NUM_NC;
        spi.xclk_source = ESP_CAM_SENSOR_XCLK_LEDC;
        spi.xclk_freq = 24000000;
        spi.xclk_pin = GPIO_NUM_NC;
#if CONFIG_CAMERA_XCLK_USE_LEDC
        spi.xclk_ledc_cfg.timer = LEDC_TIMER_0;
        spi.xclk_ledc_cfg.channel = LEDC_CHANNEL_0;
        spi.xclk_ledc_cfg.clk_cfg = LEDC_AUTO_CLK;
#endif
        esp_video_init_config_t config = {};
        config.spi = &spi;
        camera_ = new EspVideo(config);
        // Match the factory orientation after STREAMON applies the sensor format.
        const bool mirror_set = camera_->SetHMirror(false);
        const bool flip_set = camera_->SetVFlip(false);
        if (!mirror_set || !flip_set) {
            ESP_LOGW(kTag, "Failed to apply factory camera orientation");
        }
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

    void InitializeTools() {
        McpServer::GetInstance().AddTool(
            "self.system.reconfigure_wifi",
            "End this conversation and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList&) {
                Application::GetInstance().Schedule([this]() {
                    power_save_timer_->WakeUp();
                    EnterWifiConfigMode();
                });
                return true;
            });
    }

public:
    WaveshareEsp32C5TouchLcd35() {
        InitializeI2c();
        InitializePower();
        InitializeIoExpander();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeCamera();
        backlight_->RestoreBrightness();
        InitializePowerSaveTimer();
        InitializeTools();
    }

    AudioCodec* GetAudioCodec() override {
        // MCLK is not wired. ES8311 uses BCLK; EXIO5 enables the speaker amplifier.
        static Es8311AudioCodec codec(
            i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, false);
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, IO_EXPANDER_PA_ENABLE, 1));
        return &codec;
    }

    Display* GetDisplay() override { return display_; }
    Backlight* GetBacklight() override { return backlight_; }
    Camera* GetCamera() override { return camera_; }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (power_save_timer_ != nullptr && level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        const bool battery_connected = pmic_.isBatteryConnect();
        charging = battery_connected && pmic_.isCharging();
        discharging = battery_connected && pmic_.isDischarge();
        if (power_save_timer_ != nullptr) {
            power_save_timer_->SetEnabled(discharging);
        }
        if (!battery_connected) {
            return false;
        }
        level = pmic_.getBatteryPercent();
        return level >= 0;
    }
};

DECLARE_BOARD(WaveshareEsp32C5TouchLcd35);
