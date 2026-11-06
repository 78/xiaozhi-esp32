#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#ifdef CONFIG_SD_CARD_MMC_INTERFACE
#include "sdmmc.h"
#elif defined(CONFIG_SD_CARD_SPI_INTERFACE)
#include "sdspi.h"
#endif
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include <driver/i2c_master.h>
#include "codecs/box_audio_codec.h"
#include "ssid_manager.h"

#include <esp_lcd_nv3023.h>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lvgl_port.h>
#include "settings.h"
#include "pm.h"
#include "device_state_event.h"

#define TAG "OstbXiaozhi3stBoard"
static const nv3023_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xfd,(const uint8_t[]){0x06,0x08},2,0},
	{0x61,(const uint8_t[]){0x07,0x04},2,0},
	{0x62,(const uint8_t[]){0x00,0x44,0x45},3,0},
	{0x63,(const uint8_t[]){0x41,0x07,0x12,0x12},4,0},
	{0x64,(const uint8_t[]){0x37},1,0},
	{0x65,(const uint8_t[]){0x09,0x10,0x21},3,0},
	{0x66,(const uint8_t[]){0x09,0x10,0x21},3,0},
	{0x67,(const uint8_t[]){0x20,0x40},2,0},
	{0x68,(const uint8_t[]){0x90,0x4c,0x7C,0x66},4,0},
	{0xb1,(const uint8_t[]){0x0F,0x02,0x01},3,0},
	{0xB4,(const uint8_t[]){0x01},1,0},
	{0xB5,(const uint8_t[]){0x02,0x02,0x0a,0x14},4,0},
	{0xB6,(const uint8_t[]){0x04,0x01,0x9f,0x00,0x02},5,0},
	{0xdf,(const uint8_t[]){0x11},1,0},
	{0xE2,(const uint8_t[]){0x13,0x00,0x00,0x30,0x33,0x3f},6,0},
	{0xE5,(const uint8_t[]){0x3f,0x33,0x30,0x00,0x00,0x13},6,0},
	{0xE1,(const uint8_t[]){0x00,0x57},2,0},
	{0xE4,(const uint8_t[]){0x58,0x00},2,0},
	{0xE0,(const uint8_t[]){0x01,0x03,0x0d,0x0e,0x0e,0x0c,0x15,0x19},8,0},
	{0xE3,(const uint8_t[]){0x1a,0x16,0x0C,0x0f,0x0e,0x0d,0x02,0x01},8,0},
	{0xE6,(const uint8_t[]){0x00,0xff},2,0},
	{0xE7,(const uint8_t[]){0x01,0x04,0x03,0x03,0x00,0x12},6,0},
	{0xE8,(const uint8_t[]){0x00,0x70,0x00},3,0},
	{0xEc,(const uint8_t[]){0x52},1,0},
	{0xF1,(const uint8_t[]){0x01,0x01,0x02},3,0},
	{0xF6,(const uint8_t[]){0x09,0x10,0x00,0x00},4,0},
	{0xfd,(const uint8_t[]){0xfa,0xfc},2,0},
	{0x3a,(const uint8_t[]){0x05},1,0},
	{0x35,(const uint8_t[]){0x00},1,0},
	{0x36,(const uint8_t[]){0x08},1,0},
	{0x36,(const uint8_t[]){0xc8},1,0},
	{0x36,(const uint8_t[]){0x78},1,0},
	{0x36,(const uint8_t[]){0xa8},1,0},
	{0x20,(const uint8_t[]){0},0,0},
	{0x11,(const uint8_t[]){0},0,200},
	{0x29,(const uint8_t[]){0},0,10},
};

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                     int width, int height, int offset_x, int offset_y,
                     bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        DisplayLockGuard lock(this);
        // Inset status bar by 48px from left and right to prevent icons from being clipped by rounded corners
        if (status_bar_ != nullptr) {
            lv_obj_set_style_pad_left(status_bar_, 48, 0);
            lv_obj_set_style_pad_right(status_bar_, 48, 0);
        }
        // Inset content area so multiline text/chat does not touch bottom rounded corners
        if (content_ != nullptr) {
            lv_obj_set_style_pad_left(content_, 24, 0);
            lv_obj_set_style_pad_right(content_, 24, 0);
            lv_obj_set_style_pad_bottom(content_, 12, 0);
        }
        if (chat_message_label_ != nullptr) {
            lv_obj_set_width(chat_message_label_, width_ * 0.8);
        }
        // Inset low battery warning popup to stay within rounded screen boundary
        if (low_battery_popup_ != nullptr) {
            lv_obj_set_width(low_battery_popup_, LV_HOR_RES * 0.75);
            lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -12);
        }
    }
};

class OstbXiaozhi3stBoard : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    SpiLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    i2c_master_bus_handle_t i2c_bus_;
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
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

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_47);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {      
        power_save_timer_ = new PowerSaveTimer(-1, SECONDS_TO_SLEEP_MODE, SECONDS_TO_SHUTDOWN);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(0);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting sleep mode");
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            esp_lcd_panel_disp_on_off(panel_, false);
            pm_low_power_shutdown();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_HEIGHT * DISPLAY_WIDTH *sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }
    
    void InitializeButtons() {
        boot_button_.OnMultipleClick([this]() {
            ResetWifiConfiguration();
        }, 6);

        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnDoubleClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnMultipleClick([this]() {
            power_save_timer_->WakeUp();
            ResetWifiConfiguration();
        }, 5);

        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    } 

    void InitializeNv3023Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = NV3023_PANEL_IO_SPI_CONFIG(DISPLAY_CS, DISPLAY_DC, NULL, NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        nv3023_vendor_config_t vendor_config = {  // Uncomment these lines if use custom initialization commands
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(nv3023_lcd_init_cmd_t),
        };
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        
        display_ = new CustomLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = DISPLAY_SWAP_XY ? 1U : 0U,
                .mirror_x = DISPLAY_MIRROR_X ? 1U : 0U,
                .mirror_y = DISPLAY_MIRROR_Y ? 1U : 0U,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
        tp_io_config.scl_speed_hz = 400 * 1000;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.lcd_param_bits = 0;
        tp_io_config.flags.disable_control_phase = 1;

        ESP_LOGI(TAG, "Initialize touch controller");
        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Touch I2C panel IO create failed (%s), touch disabled", esp_err_to_name(ret));
            return;
        }

        esp_lcd_touch_handle_t tp = NULL;
        ret = esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "CST816S touch controller not detected (%s), touch disabled", esp_err_to_name(ret));
            return;
        }

        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lv_indev_t* indev = lvgl_port_add_touch(&touch_cfg);
        if (indev != nullptr) {
            lv_indev_add_event_cb(indev, [](lv_event_t* e) {
                auto self = static_cast<OstbXiaozhi3stBoard*>(lv_event_get_user_data(e));
                if (self->power_save_timer_) {
                    self->power_save_timer_->WakeUp();
                }
            }, LV_EVENT_PRESSED, this);
        }
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

public:
    OstbXiaozhi3stBoard():
        boot_button_(BOOT_BUTTON_GPIO, false, 0, 0, true),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO, false, 0, 0, true),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO, false, 0, 0, true) {     
        InitializeI2c();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeButtons();
        InitializeNv3023Display();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();

        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state) {
            if (current_state != kDeviceStateIdle) {
                power_save_timer_->WakeUp();
            }
        });
    }

    virtual AudioCodec* GetAudioCodec() override { 
        static BoxAudioCodec audio_codec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE,  AUDIO_OUTPUT_SAMPLE_RATE,AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }

#ifdef CONFIG_SD_CARD_MMC_INTERFACE
    virtual SdCard* GetSdCard() override {
#ifdef CARD_SDMMC_BUS_WIDTH_4BIT
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO,
                           CARD_SDMMC_D1_GPIO,
                           CARD_SDMMC_D2_GPIO,
                           CARD_SDMMC_D3_GPIO);
#else
#ifdef CARD_SDMMC_D3_GPIO
        if (CARD_SDMMC_D3_GPIO != GPIO_NUM_NC) {
            gpio_set_direction(CARD_SDMMC_D3_GPIO, GPIO_MODE_INPUT);
            gpio_pullup_en(CARD_SDMMC_D3_GPIO);
            vTaskDelay(pdMS_TO_TICKS(10)); // Wait for the pin to stabilize
        }
#endif
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO);
#endif
        return &sdmmc;
    }
#endif
#ifdef CONFIG_SD_CARD_SPI_INTERFACE
    virtual SdCard* GetSdCard() override {
        static SdSPI sdspi(CARD_SPI_MISO_GPIO,
                           CARD_SPI_MOSI_GPIO,
                           CARD_SPI_SCLK_GPIO,
                           CARD_SPI_CS_GPIO);
        return &sdspi;
    }
#endif
};

DECLARE_BOARD(OstbXiaozhi3stBoard);
