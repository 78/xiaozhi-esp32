#include "wifi_board.h"
#include "display/lcd_display.h"
// #include "font_awesome_symbols.h"

#include "codecs/box_audio_codec.h"
#include "waveshare_audio_codec.h"

#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"

#include <wifi_station.h>

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_idf_version.h>
#include <driver/i2c_master.h>
#include "i2c_bus.h"
#include "bq27220.h"

#include "settings.h"

#include <esp_lcd_touch_gt911.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#define TAG "WaveshareEsp32s3TouchLCD7c"

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_io_expander_handle_t io_handle)
        : Backlight(), io_handle_(io_handle) {}

protected:
    esp_io_expander_handle_t io_handle_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        if (brightness > 100) brightness = 100;
        int flipped_brightness = brightness;

        custom_io_expander_set_pwm(io_handle_, flipped_brightness * 255 / 100);
    }
};


class WaveshareEsp32s3TouchLCD7c : public WifiBoard {
private:
    i2c_bus_handle_t i2c_bus_handle_ = NULL;
    i2c_master_bus_handle_t i2c_bus_;
    LcdDisplay* display_;
    esp_io_expander_handle_t io_expander = NULL;
    PowerSaveTimer* power_save_timer_;
    CustomBacklight *backlight_;
    bq27220_handle_t bq27220_ = NULL;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(5);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeGpio() {
        // Zero-initialize the GPIO configuration structure
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE; // Disable interrupts for this pin
        io_conf.pin_bit_mask = 1ULL << BSP_LCD_TOUCH_INT;    // Select the GPIO pin using a bitmask
        io_conf.mode = GPIO_MODE_OUTPUT;          // Set pin as output
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE; // Disable pull-up
        gpio_config(&io_conf); // Apply the configuration
    }

    void InitializeCodecI2c() {
        i2c_config_t i2c_cfg = {};
        i2c_cfg.mode = I2C_MODE_MASTER;
        i2c_cfg.sda_io_num = BSP_I2C_SDA;
        i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_cfg.scl_io_num = BSP_I2C_SCL;
        i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_cfg.master.clk_speed = 100 * 1000;
        i2c_bus_handle_ = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
        ESP_ERROR_CHECK(i2c_bus_handle_ == NULL ? ESP_FAIL : ESP_OK);
        i2c_bus_ = i2c_bus_get_internal_bus_handle(i2c_bus_handle_);
        ESP_ERROR_CHECK(i2c_bus_ == NULL ? ESP_FAIL : ESP_OK);
    }

    void InitializeBq27220() {
        static parameter_cedv_t default_cedv = {
            .full_charge_cap = 1000,
            .design_cap = 1000,
            .reserve_cap = 0,
            .near_full = 100,
            .self_discharge_rate = 10,
            .EDV0 = 3200,
            .EDV1 = 3300,
            .EDV2 = 3400,
            .EMF = 3670,
            .C0 = 115,
            .R0 = 968,
            .T0 = 4547,
            .R1 = 4764,
            .TC = 11,
            .C1 = 0,
            .DOD0   = 4200,
            .DOD10  = 4100,
            .DOD20  = 4000,
            .DOD30  = 3920,
            .DOD40  = 3850,
            .DOD50  = 3800,
            .DOD60  = 3750,
            .DOD70  = 3700,
            .DOD80  = 3600,
            .DOD90  = 3450,
            .DOD100 = 3200,
        };
        static const gauging_config_t default_config = {
            .CCT = 1,
            .CSYNC = 0,
            .EDV_CMP = 0,
            .SC = 1,
            .FIXED_EDV0 = 0,
            .FCC_LIM = 1,
            .FC_FOR_VDQ = 1,
            .IGNORE_SD = 1,
            .SME0 = 0,
        };

        bq27220_config_t bq27220_cfg = {
            .i2c_bus = i2c_bus_handle_,
            .cfg = &default_config,
            .cedv = &default_cedv,
        };

        bq27220_ = bq27220_create(&bq27220_cfg);
        if (bq27220_ == NULL) {
            ESP_LOGW(TAG, "BQ27220 not detected, battery reporting disabled");
        }
    }

    void InitializeCustomio(void) {
        custom_io_expander_new_i2c_ch32v003(i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS, &io_expander);
        esp_io_expander_set_dir(io_expander, BSP_POWER_AMP_IO | BSP_LCD_BACKLIGHT | BSP_LCD_TOUCH_RST | IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
        esp_io_expander_set_level(io_expander, BSP_LCD_BACKLIGHT | BSP_LCD_TOUCH_RST | BSP_POWER_AMP_IO, 1);
        esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_6, 1);

        esp_io_expander_set_level(io_expander, BSP_LCD_TOUCH_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(BSP_LCD_TOUCH_INT, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_io_expander_set_level(io_expander, BSP_LCD_TOUCH_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    void InitializeRGB() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 16 * 1000 * 1000,
                .h_res = BSP_LCD_H_RES,
                .v_res = BSP_LCD_V_RES,
                .hsync_pulse_width = 4,
                .hsync_back_porch = 8,
                .hsync_front_porch = 8,
                .vsync_pulse_width = 4,
                .vsync_back_porch = 8,
                .vsync_front_porch = 8,
                .flags = {
                    .pclk_active_neg = true
                }
            },
            .data_width = 16,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 2,
            .bounce_buffer_size_px = BSP_LCD_H_RES * 10,
            .dma_burst_size = 64,
            .hsync_gpio_num = BSP_LCD_HSYNC,
            .vsync_gpio_num = BSP_LCD_VSYNC,
            .de_gpio_num = BSP_LCD_DE,
            .pclk_gpio_num = BSP_LCD_PCLK,
            .disp_gpio_num = BSP_LCD_DISP,
            .data_gpio_nums = {
                BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
                BSP_LCD_DATA4, BSP_LCD_DATA5, BSP_LCD_DATA6, BSP_LCD_DATA7,
                BSP_LCD_DATA8, BSP_LCD_DATA9, BSP_LCD_DATA10, BSP_LCD_DATA11,
                BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15
            },
            .flags = {
                .fb_in_psram = 1,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                  BSP_LCD_H_RES, BSP_LCD_V_RES, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        backlight_ = new CustomBacklight(io_expander);
        backlight_->RestoreBrightness();
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = BSP_LCD_H_RES - 1,
            .y_max = BSP_LCD_V_RES - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;

        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.scl_speed_hz = 400 * 1000;
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;

        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);

        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

public:
    WaveshareEsp32s3TouchLCD7c() {
        InitializePowerSaveTimer();
        // InitializeGpio();
        InitializeCodecI2c();
        InitializeBq27220();
        InitializeCustomio();
        InitializeRGB();
        InitializeTouch();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static WaveshareAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            BSP_I2S_MCLK,
            BSP_I2S_SCLK,
            BSP_I2S_LCLK,
            BSP_I2S_DOUT,
            BSP_I2S_DSIN,
            BSP_PA_PIN,
            BSP_CODEC_ES8389_ADDR,
            BSP_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight *GetBacklight() override {
         return backlight_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        if (bq27220_ == NULL) {
            level = 0;
            charging = false;
            discharging = false;
            return false;
        }
        charging = (bq27220_get_current(bq27220_) > 0);
        discharging = !charging;
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = bq27220_get_state_of_charge(bq27220_);
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchLCD7c);
