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
#include "M5IOE1.h"
#include "M5PM1.h"
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
        lv_obj_set_style_pad_top(status_bar_, 20, 0);
        lv_obj_set_style_pad_bottom(status_bar_, 0, 0);
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

typedef enum {
    BSP_PA_MODE_1 = 1, /*!< PA Mode 1 - 1 rising edge pulse */
    BSP_PA_MODE_2 = 2, /*!< PA Mode 2 - 2 rising edge pulses (1W speaker) */
    BSP_PA_MODE_3 = 3, /*!< PA Mode 3 - 3 rising edge pulses */
    BSP_PA_MODE_4 = 4, /*!< PA Mode 4 - 4 rising edge pulses */
} bsp_pa_mode_t;

class M5StackStopWatchBoard : public WifiBoard {
private:
    
    Button boot_button_;
    Button button2_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1* pmic_;
    M5IOE1* ioe_;
    esp_err_t PaInit(bsp_pa_mode_t mode) {
        // if (!ioe_) {
        //     ESP_LOGE(TAG, "IOE1 not initialized for PA control");
        //     return ESP_ERR_INVALID_STATE;
        // }

        // if (mode < BSP_PA_MODE_1 || mode > BSP_PA_MODE_4) {
        //     ESP_LOGE(TAG, "Invalid PA mode: %d", mode);
        //     return ESP_ERR_INVALID_ARG;
        // }

        // m5ioe1_aw8737a_pulse_num_t pulse = M5IOE1_AW8737A_PULSE_NUM_1;
        // switch (mode) {
        //     case BSP_PA_MODE_1:
        //         pulse = M5IOE1_AW8737A_PULSE_NUM_1;
        //         break;
        //     case BSP_PA_MODE_2:
        //         pulse = M5IOE1_AW8737A_PULSE_NUM_2;
        //         break;
        //     case BSP_PA_MODE_3:
        //         pulse = M5IOE1_AW8737A_PULSE_NUM_3;
        //         break;
        //     case BSP_PA_MODE_4:
        //         pulse = M5IOE1_AW8737A_PULSE_NUM_3;
        //         ESP_LOGW(TAG, "PA mode 4 maps to 3 pulses on AW8737A");
        //         break;
        //     default:
        //         return ESP_ERR_INVALID_ARG;
        // }

        // ESP_LOGI(TAG, "Setting AW8737A pulse to %d on IOE1 pin %d", pulse, AUDIO_CODEC_AW8737A_PIN);
        // m5ioe1_err_t err = ioe_->setAw8737aPulse(
        //     AUDIO_CODEC_AW8737A_PIN,
        //     pulse,
        //     M5IOE1_AW8737A_REFRESH_NOW);
        // if (err != M5IOE1_OK) {
        //     ESP_LOGE(TAG, "Failed to set AW8737A pulse: %d", err);
        //     return ESP_FAIL;
        // }

        // vTaskDelay(pdMS_TO_TICKS(10));
        // ESP_LOGI(TAG, "PA on"); 

        ioe_->pinMode(AUDIO_CODEC_AW8737A_PIN, OUTPUT);
        ioe_->setDriveMode(AUDIO_CODEC_AW8737A_PIN, M5IOE1_DRIVE_PUSHPULL);
        ioe_->digitalWrite(AUDIO_CODEC_AW8737A_PIN, HIGH); // Set PA on

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
    
        ESP_LOGI(TAG, "M5Stack PMIC Init.");
        pmic_ = new M5PM1();
        ESP_ERROR_CHECK(pmic_->begin(i2c_bus_, M5PM1_DEFAULT_ADDR));
        ESP_LOGI(TAG, "Enabling charge");
        pmic_->setChargeEnable(true);
        pmic_->setBoostEnable(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Configure PM1 G2 as input for charging status detection
        // PM1 G2: low = charging, high = not charging
        ESP_LOGI(TAG, "PM1 G2 configured as input for charging status");
        pmic_->pinMode(2, INPUT);
    
        ioe_ = new M5IOE1();
        ESP_ERROR_CHECK(ioe_->begin(i2c_bus_, M5IOE1_DEFAULT_ADDR));
        
        // Configure IOE1 G3, G5, G8 as output, high level
        ESP_LOGI(TAG, "Configuring IOE1 G3, G5, G8 as output high");
        // Audio Power enable (M5IOE1 G3)
        ioe_->pinMode(M5IOE1_PIN_3, OUTPUT);
        ioe_->setDriveMode(M5IOE1_PIN_3, M5IOE1_DRIVE_PUSHPULL);
        ioe_->digitalWrite(M5IOE1_PIN_3, HIGH);
        
        // LCD Reset (M5IOE1 G5)
        ioe_->pinMode(M5IOE1_PIN_5, OUTPUT);
        ioe_->setDriveMode(M5IOE1_PIN_5, M5IOE1_DRIVE_PUSHPULL);
        ioe_->digitalWrite(M5IOE1_PIN_5, HIGH);
        
        // LCD Power enable (M5IOE1 G8)
        ioe_->pinMode(M5IOE1_PIN_8, OUTPUT);
        ioe_->setDriveMode(M5IOE1_PIN_8, M5IOE1_DRIVE_PUSHPULL);
        ioe_->digitalWrite(M5IOE1_PIN_8, HIGH);
        vTaskDelay(pdMS_TO_TICKS(20));
   
        I2cDetect();
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
            {0x11, (uint8_t []){0x00}, 0, 120}, // Exit sleep
            {0x29, (uint8_t []){0x00}, 0, 20},  // Display on 
        };

        co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC; // Reset controlled via M5IOE1_G5
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));
 
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
        display_(nullptr),
        i2c_bus_(nullptr),
        pmic_(nullptr),
        ioe_(nullptr) {
        InitializeI2c();
        InitializeQspi();
        InitializeCo5300Display();
        InitializeButtons();
        PaInit(BSP_PA_MODE_2);  // Use mode 2 for 1W speaker
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

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!pmic_) {
            return false;
        }

        // Get battery voltage in mV
        uint16_t voltage_mv = 0;
        if (pmic_->readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        // Get charging status from PM1_G2 (low = charging, high = not charging)
        int pm1_g2_level = pmic_->digitalRead(2);
        if (pm1_g2_level < 0) {
            return false;
        }
        charging = (pm1_g2_level == 0);  // Low level means charging
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

        ESP_LOGI(TAG, "Battery: %d%% (%dmV), Charging: %s", level, voltage_mv, charging ? "Yes" : "No");
        return true;
    }
};

DECLARE_BOARD(M5StackStopWatchBoard);

