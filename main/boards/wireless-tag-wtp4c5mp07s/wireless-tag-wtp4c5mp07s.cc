#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
// #include "display/no_display.h"
#include "button.h"
#include "config.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#include "esp_lcd_ek79007.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
#include "esp_lcd_touch_gt911.h"

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#define TAG "WirelessTagEsp32p47b"

class WirelessTagEsp32p47b : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay *display_;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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
    }

    static esp_err_t bsp_enable_dsi_phy_power(void) {
#if MIPI_DSI_PHY_PWR_LDO_CHAN > 0
        // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
        static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
        ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif // BSP_MIPI_DSI_PHY_PWR_LDO_CHAN > 0

        return ESP_OK;
    }

    void InitializeLCD() {
        bsp_enable_dsi_phy_power();
        esp_lcd_panel_io_handle_t io = NULL;
        esp_lcd_panel_handle_t disp_panel = NULL;

        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = 2,
            .lane_bit_rate_mbps = 900,
        };
        esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        // we use DBI interface to send LCD commands and parameters
        esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
        esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);

        esp_lcd_dpi_panel_config_t dpi_config = {
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 52,
            .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
            .num_fbs = 1,
            .video_timing = {
                .h_size = 1024,
                .v_size = 600,
                .hsync_pulse_width = 10,
                .hsync_back_porch = 160,
                .hsync_front_porch = 160,
                .vsync_pulse_width = 1,
                .vsync_back_porch = 23,
                .vsync_front_porch = 12,
            },
            .flags = {
                .use_dma2d = true,
            },
        };
        ek79007_vendor_config_t vendor_config = {
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };

        const esp_lcd_panel_dev_config_t lcd_dev_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .flags = {
                .reset_active_high = true,
            },
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &disp_panel);
        esp_lcd_panel_reset(disp_panel);
        esp_lcd_panel_init(disp_panel);

        display_ = new MipiLcdDisplay(io, disp_panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#if 0
        lv_display_t *disp = lv_display_get_default();
        if (disp) {
            lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_180);
            ESP_LOGI(TAG, "Display rotated 180 degrees");
        } else {
            ESP_LOGE(TAG, "Failed to get default display for rotation");
        }
#endif
    }

    void InitializeTouch()
    {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_21,
            .levels = {
                .reset = 1,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeSdCard() {
#if SDCARD_SDMMC_ENABLED
        sd_pwr_ctrl_handle_t sd_ldo = NULL;
        sd_pwr_ctrl_ldo_config_t ldo_cfg = { .ldo_chan_id = 4 };
        if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &sd_ldo) == ESP_OK) {
            ESP_LOGI(TAG, "SD LDO channel 4 enabled");
        } else {
            ESP_LOGW(TAG, "Failed to enable SD LDO channel 4");
        }
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        // Map pins via GPIO matrix if needed
        slot_config.clk = SDCARD_SDMMC_CLK_PIN;
        slot_config.cmd = SDCARD_SDMMC_CMD_PIN;
        slot_config.d0 = SDCARD_SDMMC_D0_PIN;
        slot_config.width = SDCARD_SDMMC_BUS_WIDTH;
        if (SDCARD_SDMMC_BUS_WIDTH == 4) {
            slot_config.d1 = SDCARD_SDMMC_D1_PIN;
            slot_config.d2 = SDCARD_SDMMC_D2_PIN;
            slot_config.d3 = SDCARD_SDMMC_D3_PIN;
        }

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 0,
            .disk_status_check_enable = true,
        };
        sdmmc_card_t* card;
        host.pwr_ctrl_handle = sd_ldo;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
        if (ret == ESP_OK) {
            sdmmc_card_print_info(stdout, card);
            ESP_LOGI(TAG, "SD card mounted at %s (SDMMC)", SDCARD_MOUNT_POINT);
        } else {
            ESP_LOGW(TAG, "Failed to mount SD card (SDMMC): %s", esp_err_to_name(ret));
        }
#elif SDCARD_SDSPI_ENABLED
        sd_pwr_ctrl_handle_t sd_ldo = NULL;
        sd_pwr_ctrl_ldo_config_t ldo_cfg = { .ldo_chan_id = 4 };
        if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &sd_ldo) == ESP_OK) {
            ESP_LOGI(TAG, "SD LDO channel 4 enabled");
        } else {
            ESP_LOGW(TAG, "Failed to enable SD LDO channel 4");
        }
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = SDCARD_SPI_MOSI,
            .miso_io_num = SDCARD_SPI_MISO,
            .sclk_io_num = SDCARD_SPI_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize((spi_host_device_t)SDCARD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = SDCARD_SPI_CS;
        slot_config.host_id = (spi_host_device_t)SDCARD_SPI_HOST;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 0,
            .disk_status_check_enable = true,
        };
        sdmmc_card_t* card;
        host.pwr_ctrl_handle = sd_ldo;
        esp_err_t ret = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
        if (ret == ESP_OK) {
            sdmmc_card_print_info(stdout, card);
            ESP_LOGI(TAG, "SD card mounted at %s (SDSPI)", SDCARD_MOUNT_POINT);
        } else {
            ESP_LOGW(TAG, "Failed to mount SD card (SDSPI): %s", esp_err_to_name(ret));
        }
#else
        ESP_LOGI(TAG, "SD card disabled (enable SDCARD_SDMMC_ENABLED or SDCARD_SDSPI_ENABLED)");
#endif
    }

public:
    WirelessTagEsp32p47b() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeLCD();
        InitializeTouch();
        InitializeSdCard();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

};

DECLARE_BOARD(WirelessTagEsp32p47b);
