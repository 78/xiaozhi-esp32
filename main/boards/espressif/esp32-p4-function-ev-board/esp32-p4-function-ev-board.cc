#include "application.h"
#include "audio/codecs/es8311_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/display.h"
#include "display/lcd_display.h"
#include "esp_video.h"
#include "lvgl_theme.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <driver/sdmmc_host.h>
#include <esp_idf_version.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#define TAG "ESP32P4FuncEV"

class ESP32P4FunctionEvBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    esp_lcd_dsi_bus_handle_t dsi_bus_ = nullptr;
    esp_ldo_channel_handle_t dsi_phy_power_ = nullptr;
    esp_lcd_touch_handle_t tp_ = nullptr;
    esp_lcd_panel_io_handle_t touch_io_ = nullptr;
    lv_indev_t* touch_indev_ = nullptr;
    EspVideo* camera_ = nullptr;
    sdmmc_card_t* sd_card_ = nullptr;
    sd_pwr_ctrl_handle_t sd_power_ = nullptr;
    bool sd_card_mounted_ = false;

    void InitializeI2cBus() {
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = AUDIO_CODEC_I2C_PORT,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &codec_i2c_bus_));
    }

    void InitializeLcd() {
        esp_ldo_channel_config_t ldo_config = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &dsi_phy_power_));

        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = LCD_MIPI_DSI_LANE_NUM,
            .lane_bit_rate_mbps = LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus_));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus_, &dbi_config, &panel_io));

        esp_lcd_dpi_panel_config_t dpi_config =
            EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
        dpi_config.num_fbs = 1;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        dpi_config.flags.use_dma2d = true;
#endif

        ek79007_vendor_config_t vendor_config = {
            .mipi_config =
                {
                    .dsi_bus = dsi_bus_,
                    .dpi_config = &dpi_config,
                },
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(panel_io, &panel_config, &panel));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d(panel));
#endif
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

        display_ = new MipiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                      DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
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

    void InitializeTouch() {
        esp_lcd_touch_config_t touch_config = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    .swap_xy = 0,
                    .mirror_x = 1,
                    .mirror_y = 1,
                },
        };
        esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        touch_io_config.scl_speed_hz = 400000;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &touch_io_config, &touch_io_));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(touch_io_, &touch_config, &tp_));

        lv_display_t* lv_display = lv_display_get_default();
        if (lv_display == nullptr) {
            ESP_LOGE(TAG, "Cannot register touch input without an LVGL display");
            return;
        }
        const lvgl_port_touch_cfg_t lv_touch_config = {
            .disp = lv_display,
            .handle = tp_,
        };
        touch_indev_ = lvgl_port_add_touch(&lv_touch_config);
        if (touch_indev_ == nullptr) {
            ESP_LOGE(TAG, "Failed to register GT911 touch input");
        }
    }

    void InitializeSdCard() {
        ESP_LOGI(TAG, "Initializing SD card");

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

        sdmmc_slot_config_t slot = {};
        slot.cd = SDMMC_SLOT_NO_CD;
        slot.wp = SDMMC_SLOT_NO_WP;
        slot.width = 4;

        const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 64 * 1024,
        };

        sd_pwr_ctrl_ldo_config_t power_config = {
            .ldo_chan_id = SD_CARD_PWR_LDO_CHAN,
        };
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&power_config, &sd_power_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable SD card power: %s", esp_err_to_name(ret));
            return;
        }
        host.pwr_ctrl_handle = sd_power_;

        ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot, &mount_config, &sd_card_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
            sd_pwr_ctrl_del_on_chip_ldo(sd_power_);
            sd_power_ = nullptr;
        } else {
            sd_card_mounted_ = true;
            ESP_LOGI(TAG, "SD card mounted successfully");
        }
    }

    void InitializeCamera() {
        ESP_LOGI(TAG, "Initializing camera");

        esp_video_init_csi_config_t csi_config = {
            .sccb_config =
                {
                    .init_sccb = false,
                    .i2c_handle = codec_i2c_bus_,
                    .freq = 400000,
                },
            .reset_pin = CAMERA_RESET_PIN,
            .pwdn_pin = CAMERA_PWDN_PIN,
        };
        esp_video_init_config_t video_config = {
            .csi = &csi_config,
        };

        camera_ = new EspVideo(video_config);
    }

    void InitializeFonts() {
        ESP_LOGI(TAG, "Initializing font support");
        auto& theme_manager = LvglThemeManager::GetInstance();
        auto current_theme = theme_manager.GetTheme("light");
        if (current_theme != nullptr) {
            auto text_font = current_theme->text_font();
            if (text_font != nullptr && text_font->font() != nullptr) {
                ESP_LOGI(TAG, "Custom font loaded successfully: line_height=%d",
                         text_font->font()->line_height);
            } else {
                ESP_LOGW(TAG, "Custom font not loaded, using built-in font");
            }
        }
    }

public:
    ESP32P4FunctionEvBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2cBus();
        InitializeLcd();
        InitializeButtons();
        InitializeTouch();
        InitializeSdCard();
        InitializeCamera();
        InitializeFonts();
        GetBacklight()->RestoreBrightness();
    }

    ~ESP32P4FunctionEvBoard() {
        delete camera_;
        camera_ = nullptr;

        if (sd_card_mounted_) {
            esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, sd_card_);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
            }
            sd_card_mounted_ = false;
            sd_card_ = nullptr;
        }
        if (sd_power_ != nullptr) {
            sd_pwr_ctrl_del_on_chip_ldo(sd_power_);
            sd_power_ = nullptr;
        }

        if (touch_indev_ != nullptr) {
            lvgl_port_remove_touch(touch_indev_);
            touch_indev_ = nullptr;
        }
        if (tp_ != nullptr) {
            esp_lcd_touch_del(tp_);
            tp_ = nullptr;
        }
        if (touch_io_ != nullptr) {
            esp_lcd_panel_io_del(touch_io_);
            touch_io_ = nullptr;
        }

        delete display_;
        display_ = nullptr;

        if (dsi_bus_ != nullptr) {
            esp_lcd_del_dsi_bus(dsi_bus_);
            dsi_bus_ = nullptr;
        }
        if (dsi_phy_power_ != nullptr) {
            esp_ldo_release_channel(dsi_phy_power_);
            dsi_phy_power_ = nullptr;
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, AUDIO_CODEC_I2C_PORT, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, ES8311_CODEC_DEFAULT_ADDR, true, false);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override { return camera_; }
};

DECLARE_BOARD(ESP32P4FunctionEvBoard);
