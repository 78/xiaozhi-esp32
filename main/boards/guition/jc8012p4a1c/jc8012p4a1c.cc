#include "sdkconfig.h"

#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "backlight.h"

#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_ldo_regulator.h>
#include <esp_lcd_jd9365.h>
#include <esp_lvgl_port.h>
#include <driver/i2c_master.h>
#include "esp_lcd_gsl3680.h"
#include "lcd_init_cmds.h"

#if CONFIG_JC8012P4A1C_ENABLE_CAMERA
#include "esp_video.h"
#endif

#define TAG "JC8012P4A1C"

class JC8012P4A1C : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
#if CONFIG_JC8012P4A1C_ENABLE_CAMERA
    EspVideo* camera_ = nullptr;
#endif

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = BOARD_I2C_SDA_PIN,
            .scl_io_num = BOARD_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    static void EnableDsiPhyPower() {
        static esp_ldo_channel_handle_t phy_pwr_chan = nullptr;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan));
        ESP_LOGI(TAG, "MIPI DSI PHY powered on");
    }

    void InitializeDisplay() {
        EnableDsiPhyPower();

        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = LCD_MIPI_DSI_LANE_NUM,
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io));

        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = LCD_DPI_CLOCK_FREQ_MHZ,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 2,
            .video_timing = {
                .h_size = DISPLAY_WIDTH,
                .v_size = DISPLAY_HEIGHT,
                .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
                .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
                .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
                .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
                .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
                .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
            },
        };

        jd9365_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
                .lane_num = LCD_MIPI_DSI_LANE_NUM,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = LCD_BIT_PER_PIXEL,
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .vendor_config = &vendor_config,
        };

        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new MipiLcdDisplay(io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                      DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        if (i2c_master_probe(i2c_bus_, ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS, 100) != ESP_OK) {
            ESP_LOGW(TAG, "GSL3680 not found at 0x%02X, continuing without touch",
                     ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS);
            return;
        }

        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
        tp_io_config.scl_speed_hz = 400 * 1000;

        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        if (esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create touch panel IO");
            return;
        }

        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = TOUCH_RST_PIN,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 1,
            },
        };

        esp_lcd_touch_handle_t tp = nullptr;
        if (esp_lcd_touch_new_i2c_gsl3680(tp_io_handle, &tp_cfg, &tp) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize GSL3680");
            return;
        }

        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "GSL3680 touch initialized");
    }

#if CONFIG_JC8012P4A1C_ENABLE_CAMERA
    void InitializeCamera() {
        esp_video_init_csi_config_t csi_config = {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = i2c_bus_,
                .freq = 400000,
            },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin = GPIO_NUM_NC,
        };
        esp_video_init_config_t cam_config = {
            .csi = &csi_config,
        };
        camera_ = new EspVideo(cam_config);
    }
#endif

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
    JC8012P4A1C() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeDisplay();
        InitializeTouch();
#if CONFIG_JC8012P4A1C_ENABLE_CAMERA
        InitializeCamera();
#endif
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static GpioBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

#if CONFIG_JC8012P4A1C_ENABLE_CAMERA
    virtual Camera* GetCamera() override {
        return camera_;
    }
#endif
};

DECLARE_BOARD(JC8012P4A1C);
