#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "backlight.h"
#include "m5stack_ioe1.h"
#include "M5PM1.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_st7102.h"
#include <esp_lvgl_port.h>

#define TAG "M5StackCoreP4"

static const st7102_lcd_init_cmd_t st7102_init_cmds[] = {
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA2}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA3}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA4}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x78, .data = (uint8_t[]){0x21}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x79, .data = (uint8_t[]){0xCF}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xB0, .data = (uint8_t[]){0x22, 0x43, 0x1E, 0x43, 0x2F, 0x57, 0x57}, .data_bytes = 7, .delay_ms = 0},
    {.cmd = 0xB7, .data = (uint8_t[]){0x7D, 0x7D}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x7A, 0x7A}, .data_bytes = 2, .delay_ms = 0},
    {.cmd        = 0xC8,
     .data       = (uint8_t[]){0x00, 0x00, 0x13, 0x23, 0x3E, 0x00, 0x6A, 0x03, 0xB0, 0x06, 0x11, 0x0F, 0x07,
                                      0x85, 0x03, 0x21, 0xD5, 0x01, 0x18, 0x00, 0x22, 0x56, 0x0F, 0x98, 0x0A, 0x32,
                                      0xF8, 0x0D, 0x48, 0x0F, 0xF3, 0x80, 0x0F, 0xAC, 0xC1, 0x03, 0xC4},
     .data_bytes = 37,
     .delay_ms   = 0},
    {.cmd        = 0xC9,
     .data       = (uint8_t[]){0x00, 0x00, 0x13, 0x23, 0x3E, 0x00, 0x6A, 0x03, 0xB0, 0x06, 0x11, 0x0F, 0x07,
                                      0x85, 0x03, 0x21, 0xD5, 0x01, 0x18, 0x00, 0x22, 0x56, 0x0F, 0x98, 0x0A, 0x32,
                                      0xF8, 0x0D, 0x48, 0x0F, 0xF3, 0x80, 0x0F, 0xAC, 0xC1, 0x03, 0xC4},
     .data_bytes = 37,
     .delay_ms   = 0},
    {.cmd = 0xD7, .data = (uint8_t[]){0x10, 0x0C, 0x02, 0x19, 0x40, 0x40}, .data_bytes = 6, .delay_ms = 0},
    {.cmd        = 0xA3,
     .data       = (uint8_t[]){0x40, 0x03, 0x80, 0xCF, 0x44, 0x00, 0x00, 0x00, 0x02, 0x05, 0x6F,
                                      0x6F, 0x00, 0x1A, 0x00, 0x45, 0x05, 0x00, 0x00, 0x00, 0x00, 0x46,
                                      0x00, 0x00, 0x02, 0x20, 0x52, 0x00, 0x05, 0x00, 0x00, 0xFF},
     .data_bytes = 32,
     .delay_ms   = 0},
    {.cmd = 0xA6,
     .data =
         (uint8_t[]){0x02, 0x00, 0x24, 0x55, 0x35, 0x00, 0x38, 0x00, 0x97, 0x97, 0x00, 0x24, 0x55, 0x36, 0x00,
                            0x37, 0x00, 0x97, 0x97, 0x02, 0xAC, 0x51, 0x3A, 0x00, 0x00, 0x00, 0x97, 0x97, 0x00, 0xAC,
                            0x21, 0x00, 0x0B, 0x00, 0x00, 0x97, 0x97, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00},
     .data_bytes = 44,
     .delay_ms   = 0},
    {.cmd        = 0xA7,
     .data       = (uint8_t[]){0x19, 0x19, 0x00, 0x64, 0x40, 0x07, 0x16, 0x40, 0x00, 0x04, 0x03, 0x97,
                                      0x97, 0x00, 0x64, 0x40, 0x25, 0x34, 0x00, 0x00, 0x02, 0x01, 0x97, 0x97,
                                      0x00, 0x64, 0x40, 0x4B, 0x5A, 0x00, 0x00, 0x02, 0x01, 0x97, 0x97, 0x00,
                                      0x24, 0x40, 0x69, 0x78, 0x00, 0x00, 0x00, 0x00, 0x97, 0x97, 0x00, 0x44},
     .data_bytes = 48,
     .delay_ms   = 0},
    {.cmd        = 0xAC,
     .data       = (uint8_t[]){0x11, 0x08, 0x13, 0x0A, 0x18, 0x1A, 0x1B, 0x00, 0x06, 0x03, 0x19, 0x1B, 0x1B,
                                      0x1B, 0x18, 0x1B, 0x10, 0x09, 0x12, 0x0B, 0x18, 0x1A, 0x1B, 0x02, 0x06, 0x01,
                                      0x19, 0x1B, 0x1B, 0x1B, 0x18, 0x1B, 0xFF, 0x67, 0xFF, 0x67, 0x00},
     .data_bytes = 37,
     .delay_ms   = 0},
    {.cmd = 0xAD, .data = (uint8_t[]){0xCC, 0x40, 0x46, 0x11, 0x04, 0x6F, 0x6F}, .data_bytes = 7, .delay_ms = 0},
    {.cmd  = 0xE8,
     .data = (uint8_t[]){0x30, 0x07, 0x00, 0xB3, 0xB3, 0x9C, 0x00, 0xE2, 0x04, 0x00, 0x00, 0x00, 0x00, 0xEF},
     .data_bytes = 14,
     .delay_ms   = 0},
    {.cmd = 0x75, .data = (uint8_t[]){0x03, 0x04}, .data_bytes = 2, .delay_ms = 0},
    {.cmd        = 0xE7,
     .data       = (uint8_t[]){0x8B, 0x3C, 0x00, 0x0C, 0xF0, 0x5D, 0x00, 0x5D, 0x00, 0x5D, 0x00,
                                      0x5D, 0x00, 0xFF, 0x00, 0x08, 0x7B, 0x00, 0x00, 0xC8, 0x6A, 0x5A,
                                      0x08, 0x1A, 0x3C, 0x00, 0x71, 0x01, 0x8C, 0x01, 0x7F, 0xF0, 0x22},
     .data_bytes = 33,
     .delay_ms   = 0},
    {.cmd        = 0xE9,
     .data       = (uint8_t[]){0x3C, 0x7F, 0x08, 0x07, 0x1A, 0x7A, 0x22, 0x1A, 0x33},
     .data_bytes = 9,
     .delay_ms   = 0},
    {.cmd = 0x11, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = 0x36, .data = (uint8_t[]){0b11}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x29, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x29, .data = NULL, .data_bytes = 0, .delay_ms = 0}
};

class M5IoE1Backlight : public Backlight {
public:
    explicit M5IoE1Backlight(m5ioe1_handle_t ioe) : ioe_(ioe) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        if (!ioe_) {
            return;
        }
        // m5ioe1_pwm_set_duty expects duty cycle 0-100
        m5ioe1_pwm_set_duty(ioe_, M5IOE1_PWM_CH1, brightness);
        brightness_ = brightness;
    }

private:
    m5ioe1_handle_t ioe_;
};

class M5StackCoreP4Board : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    m5ioe1_handle_t ioe_;
    M5PM1* pmic_;
    M5IoE1Backlight* backlight_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = SYS_I2C_PORT,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 0,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
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

    void InitializePm1() {
        ESP_LOGI(TAG, "M5Stack PMIC Init.");
        pmic_ = new M5PM1();
        pmic_->begin(i2c_bus_, 0x6F);  // M5STP2 address according to README
        pmic_->setChargeEnable(true);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    void InitializeIoExpander() {
        ioe_ = m5ioe1_create(i2c_bus_, IOE1_I2C_ADDR);

        // LCD Power enable
        m5ioe1_pin_mode(ioe_, IOE1_PIN_LCD_PWR, true);
        m5ioe1_set_drive_mode(ioe_, IOE1_PIN_LCD_PWR, false); // false = 推挽输出
        m5ioe1_digital_write(ioe_, IOE1_PIN_LCD_PWR, true);

        // LCD Reset
        m5ioe1_pin_mode(ioe_, IOE1_PIN_LCD_RST, true);
        m5ioe1_set_drive_mode(ioe_, IOE1_PIN_LCD_RST, false); // false = 推挽输出
        // m5ioe1_digital_write(ioe_, IOE1_PIN_LCD_RST, false);
        // vTaskDelay(pdMS_TO_TICKS(10));
        m5ioe1_digital_write(ioe_, IOE1_PIN_LCD_RST, true);
        vTaskDelay(pdMS_TO_TICKS(20));

        // Audio Power enable
        m5ioe1_pin_mode(ioe_, IOE1_PIN_AUDIO_PWR, true);
        m5ioe1_set_drive_mode(ioe_, IOE1_PIN_AUDIO_PWR, false); // false = 推挽输出
        m5ioe1_digital_write(ioe_, IOE1_PIN_AUDIO_PWR, true);

        // PA Enable
        m5ioe1_pin_mode(ioe_, IOE1_PIN_PA_EN, true);
        m5ioe1_set_drive_mode(ioe_, IOE1_PIN_PA_EN, false); // false = 推挽输出
        m5ioe1_digital_write(ioe_, IOE1_PIN_PA_EN, true);

        // LCD Backlight (PWM)
        m5ioe1_pin_mode(ioe_, IOE1_PIN_LCD_BL, true);
        m5ioe1_set_drive_mode(ioe_, IOE1_PIN_LCD_BL, false); // false = 推挽输出
        m5ioe1_digital_write(ioe_, IOE1_PIN_LCD_BL, true);
        m5ioe1_pwm_set_frequency(ioe_, 1000);
        m5ioe1_pwm_config(ioe_, M5IOE1_PWM_CH1, 0, M5IOE1_PWM_POLARITY_HIGH, true);
    }

    static esp_err_t bsp_enable_dsi_phy_power() {
        static esp_ldo_channel_handle_t phy_pwr_chan = nullptr;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
        ESP_LOGI(TAG, "MIPI DSI PHY powered on");
        return ESP_OK;
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;

        ESP_ERROR_CHECK(bsp_enable_dsi_phy_power());

        /* create MIPI DSI bus first, it will initialize the DSI PHY as well */
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = DISPLAY_MIPI_LANE_NUM,
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = DISPLAY_MIPI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        // we use DBI interface to send LCD commands and parameters
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,  // according to the LCD spec
            .lcd_param_bits = 8,  // according to the LCD spec
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io));

        // Wait for MIPI DSI PHY to be ready
        vTaskDelay(pdMS_TO_TICKS(50));

        ESP_LOGI(TAG, "Install LCD driver of st7102");
        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = DISPLAY_PIXEL_CLOCK_MHZ,  // 480x480 60Hz RGB565
            .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,  // RGB565 for 16 bits_per_pixel
            .num_fbs = 2,  // Double buffering for better performance
            .video_timing = {
                .h_size = DISPLAY_WIDTH,
                .v_size = DISPLAY_HEIGHT,
                .hsync_pulse_width = DISPLAY_HSYNC_PW,
                .hsync_back_porch = DISPLAY_HSYNC_BP,
                .hsync_front_porch = DISPLAY_HSYNC_FP,
                .vsync_pulse_width = DISPLAY_VSYNC_PW,
                .vsync_back_porch = DISPLAY_VSYNC_BP,
                .vsync_front_porch = DISPLAY_VSYNC_FP,
            },
            .flags = {
                .use_dma2d = true,
            },
        };

        st7102_vendor_config_t vendor_config = {
            .init_cmds = st7102_init_cmds,
            .init_cmds_size = sizeof(st7102_init_cmds) / sizeof(st7102_init_cmds[0]),
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };

        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = GPIO_NUM_NC,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_config,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7102(io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        ESP_LOGI(TAG, "Display initialized with resolution %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);

        display_ = new MipiLcdDisplay(io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                      DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
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
    M5StackCoreP4Board() :
        boot_button_(BOOT_BUTTON_GPIO),
        display_(nullptr),
        i2c_bus_(nullptr),
        ioe_(nullptr),
        pmic_(nullptr),
        backlight_(nullptr) {
        InitializeI2c();
        I2cDetect();
        InitializePm1();
        InitializeIoExpander();
        InitializeDisplay();
        InitializeButtons();
        backlight_ = new M5IoE1Backlight(ioe_);
        backlight_->SetBrightness(90);
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            SYS_I2C_PORT,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA,
            AUDIO_CODEC_ES8311_ADDR,
            true,
            false);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        return backlight_;
    }
};

DECLARE_BOARD(M5StackCoreP4Board);
