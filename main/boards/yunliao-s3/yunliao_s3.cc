#include "lvgl_theme.h"
#include "dual_network_board.h"
#include "codecs/es8388_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "power_manager.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>


#define TAG "YunliaoS3"

class YunliaoS3 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    SpiLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 600);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            power_manager_->Sleep();
        });
        power_save_timer_->SetEnabled(true);
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_PIN_MOSI;
        buscfg.miso_io_num = DISPLAY_SPI_PIN_MISO;
        buscfg.sclk_io_num = DISPLAY_SPI_PIN_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            ESP_LOGI(TAG, "Button OnDoubleClick");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });  
        boot_button_.OnMultipleClick([this]() {
            ESP_LOGI(TAG, "Button OnThreeClick");
            if (GetNetworkType() == NetworkType::WIFI) {
                auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                wifi_board.ResetWifiConfiguration();
            }
        },3);  
        boot_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "Button LongPress to Sleep");
            display_->SetStatus(Lang::Strings::PLEASE_WAIT);
            vTaskDelay(pdMS_TO_TICKS(2000));
            power_manager_->Sleep();
        });    
    }
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_PIN_LCD_CS;
        io_config.dc_gpio_num = DISPLAY_SPI_PIN_LCD_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = DISPLAY_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_LCD_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_PIN_LCD_RST;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER_COLOR;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH,
                                     DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                     DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        auto& theme_manager = LvglThemeManager::GetInstance();
        auto theme = theme_manager.GetTheme("dark");
        if (theme != nullptr) {
            display_->SetTheme(theme);
        }
    }

public:
    YunliaoS3() :
        DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC, 0),
        boot_button_(BOOT_BUTTON_PIN),
        power_manager_(new PowerManager()){
        power_manager_->Start5V();
        power_manager_->Initialize();
        InitializeI2c();
        power_manager_->CheckStartup();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeSt7789Display();
        power_manager_->OnChargingStatusDisChanged([this](bool is_discharging) {
            if(power_save_timer_){
                if (is_discharging) {
                    power_save_timer_->SetEnabled(true);
                } else {
                    power_save_timer_->SetEnabled(false);
                }
            }
        });
        if(GetNetworkType() == NetworkType::WIFI){
            power_manager_->Shutdown4G();
        }else{
            power_manager_->Start4G();
        }
        GetBacklight()->RestoreBrightness();
        while(gpio_get_level(BOOT_BUTTON_PIN) == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        InitializeButtons();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8388AudioCodec audio_codec(
            codec_i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8388_ADDR,
            AUDIO_INPUT_REFERENCE
        );
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
        level = power_manager_->GetBatteryLevel();
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(YunliaoS3);
