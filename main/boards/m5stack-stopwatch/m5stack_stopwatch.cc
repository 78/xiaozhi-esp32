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
#include <esp_lcd_co5300.h>
#include <driver/spi_common.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_manager.h>
#include "m5stack_ioe1.h"
#include "m5stack_pm1.h"
#include "esp_timer.h"
#include "lvgl.h"

#define TAG "M5StackStopWatchBoard"

// Custom display class for circular screen with coordinate rounding
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t*)lv_event_get_param(e);
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;
        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;
        
        // Round the start of coordinate down to the nearest 2M number
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        // Round the end of coordinate up to the nearest 2N+1 number
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES*0.2, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES*0.2, 0);
        lv_obj_set_style_pad_top(status_bar_, 30, 0);
        lv_obj_set_style_pad_bottom(status_bar_, 0, 0);
        // lv_obj_set_style_pad_bottom(content_, LV_HOR_RES*0.1, 0);
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

typedef enum {
    BSP_PA_MODE_1 = 1, /*!< PA Mode 1 - 1 rising edge pulse */
    BSP_PA_MODE_2 = 2, /*!< PA Mode 2 - 2 rising edge pulses (1W speaker) */
    BSP_PA_MODE_3 = 3, /*!< PA Mode 3 - 3 rising edge pulses */
    BSP_PA_MODE_4 = 4, /*!< PA Mode 4 - 4 rising edge pulses */
} bsp_pa_mode_t;

// AMOLED displays are self-emissive and don't need backlight control
// This is a dummy implementation to satisfy the framework interface
class DummyBacklight : public Backlight {
public:
    DummyBacklight() : Backlight() {}

    void SetBrightnessImpl(uint8_t brightness) override {
        // AMOLED doesn't need backlight control, do nothing
        brightness_ = brightness;
    }
};

class M5StackStopWatchBoard : public WifiBoard {
private:
    
    Button boot_button_;
    Button button2_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    m5pm1_handle_t pmic_;
    m5ioe1_handle_t ioe_;
    DummyBacklight* backlight_;
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
        ESP_LOGI(TAG, "PMIC Version: %d.%d", m5pm1_get_hw_revision(pmic_) >> 4, m5pm1_get_hw_revision(pmic_) & 0x0F);
        ESP_LOGI(TAG, "Enabling charge");
        m5pm1_set_charging(pmic_, true);
        m5pm1_set_5v_boost(pmic_, true);
        
        // Configure PM1_G2 as input for charging status detection (low = charging)
        // PM1_G2 is already configured as input by default, no need to configure

        // Audio Power enable (PYIO_G3 equivalent)
        m5ioe1_pin_mode(ioe_, 3, true);
        m5ioe1_set_drive_mode(ioe_, 3, false);  // 推挽输出
        m5ioe1_digital_write(ioe_, 3, true);
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

    void InitializeQspi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_QSPI_SCK;
        buscfg.data0_io_num = DISPLAY_QSPI_D0;
        buscfg.data1_io_num = DISPLAY_QSPI_D1;
        buscfg.data2_io_num = DISPLAY_QSPI_D2;
        buscfg.data3_io_num = DISPLAY_QSPI_D3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeCo5300Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Enabling LCD feature...");
        // OLED VBat Power enable - M5IOE1_G8
        m5ioe1_pin_mode(ioe_, 8, true);
        m5ioe1_set_drive_mode(ioe_, 8, false); // 推挽输出
        m5ioe1_digital_write(ioe_, 8, true);
        // OLED Reset - M5IOE1_G5
        m5ioe1_pin_mode(ioe_, 5, true);
        m5ioe1_set_drive_mode(ioe_, 5, false); // 推挽输出
        m5ioe1_digital_write(ioe_, 5, false); 
        vTaskDelay(pdMS_TO_TICKS(10));
        m5ioe1_digital_write(ioe_, 5, true);
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_LOGI(TAG, "Install panel IO (QSPI)");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_QSPI_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 60 * 1000 * 1000; 
        io_config.trans_queue_depth = 20;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;  // Enable QSPI mode
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &panel_io));

        ESP_LOGI(TAG, "Install LCD driver (CO5300)");
        
        // CO5300 初始化命令序列
        // 分辨率: 466x466 (圆形显示)
        static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
            // {cmd, { data }, data_size, delay_ms}
            {0xFE, (uint8_t []){0x00}, 0, 0},
            {0xC4, (uint8_t []){0x80}, 1, 0},
            {0x3A, (uint8_t []){0x55}, 0, 10}, // RGB565
            {0x35, (uint8_t []){0x00}, 0, 10},
            {0x53, (uint8_t []){0x20}, 1, 10},
            {0x51, (uint8_t []){0xFF}, 1, 10},
            {0x63, (uint8_t []){0xFF}, 1, 10},
            {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Column address: 0-465 (466 pixels: 0x0000 to 0x01D1)
            {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Row address: 0-465 (466 pixels: 0x0000 to 0x01D1)
            {0x11, (uint8_t []){0x00}, 0, 120}, // Exit sleep (增加延迟确保稳定)
            {0x29, (uint8_t []){0x00}, 0, 20},  // Display on (增加延迟)
        };

        co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;  // Reset controlled via IOE1
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));

        // Hardware reset already done via IOE1_G5
        // Since reset_gpio_num = GPIO_NUM_NC, this will execute software reset command
        ESP_LOGI(TAG, "Resetting CO5300 panel...");
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_set_gap(panel, 7, 0); // (480-466)/2 = 7
        esp_lcd_panel_disp_on_off(panel, true);
        
        ESP_LOGI(TAG, "CO5300 panel initialized successfully");

        display_ = new CustomLcdDisplay(panel_io, panel,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                        DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        // Button 1 (GPIO 1): Wake up conversation
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });

        // Button 2 (GPIO 4): Adjust volume
        button2_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        button2_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
    }

public:
    M5StackStopWatchBoard() :
        boot_button_(GPIO_NUM_1),
        button2_(BUTTON_2_GPIO),
        pmic_(nullptr),
        ioe_(nullptr),
        backlight_(nullptr) {
        InitializeI2c();
        InitializeQspi();
        InitializeCo5300Display();
        InitializeButtons();
        bsp_audio_set_pa_mode(BSP_PA_MODE_2);  // Use mode 2 for 1W speaker
        // Initialize dummy backlight (AMOLED doesn't need it, but keep for compatibility)
        backlight_ = new DummyBacklight();
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

        // Get charging status from PM1_G2 (low = charging, high = not charging)
        bool pm1_g2_level = m5pm1_digital_read(pmic_, 2);
        charging = !pm1_g2_level;  // Low level means charging
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

DECLARE_BOARD(M5StackStopWatchBoard);

