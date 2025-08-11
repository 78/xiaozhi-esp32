#include "ml307_board.h"
#include "codecs/box_audio_codec.h"
#include "codecs/es8311_audio_codec.h"

#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "led/circular_strip.h"
#include "assets/lang_config.h"
#include "power_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <driver/i2c_master.h>
#include "settings.h"

#define TAG "XINGZHI_CUBE_2_0TFT_4G"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);


class XINGZHI_CUBE_2_0TFT_4G : public Ml307Board {
private:
    Button boot_button_;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_;

    // 添加一个标志位来记录扫描结果
    esp_err_t err;
    bool is_device_41_found = false;
    bool is_device_18_found = false;

    void InitializeI2c() {
        ESP_LOGI("I2C", "Scanning I2C devices...");
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

        for (uint8_t addr = 1; addr < 127; addr++) {
            err = i2c_master_probe(i2c_bus_, addr, 100);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Device found at address 0x%02X", addr);
                if (addr == 0x41) {
                    is_device_41_found = true;
                }
                if (addr == 0x18) {
                    is_device_18_found = true;
                }
            }
        }
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(POWER_USB_IN);  // usb是否插入引脚
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
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
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            power_save_timer_->WakeUp();
            app.ToggleChatState();
        });
    }

    void InitializeSt7789Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, 
        {
            .text_font = &font_puhui_20_4,
            .icon_font = &font_awesome_20_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
            .emoji_font = font_emoji_32_init(),
#else
            .emoji_font = font_emoji_64_init(),
#endif
        });
    }

    void InitializeGpio() {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << NETWORK_MODULE_POWER_IN);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
        gpio_config(&io_conf);
        gpio_set_level(NETWORK_MODULE_POWER_IN, 1); 
    }

public:
    XINGZHI_CUBE_2_0TFT_4G() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN),
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeGpio();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeButtons();
        InitializeSt7789Display();  
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        if (!is_device_41_found && is_device_18_found){ // 无7210 有8311 则只初始化8311
            static Es8311AudioCodec audio_codec(
                i2c_bus_, 
                I2C_NUM_0,
                AUDIO_INPUT_SAMPLE_RATE, 
                AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCK, 
                AUDIO_I2S_GPIO_BCLK, 
                AUDIO_I2S_GPIO_WS, 
                AUDIO_I2S_GPIO_DOUT, 
                AUDIO_I2S_GPIO_DIN,
                AUDIO_CODEC_PA_PIN, 
                AUDIO_CODEC_ES8311_ADDR);

            return &audio_codec;
        }

        static BoxAudioCodec audio_codec( // 有7210 有8311 则都初始化
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);

        return &audio_codec;
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, 3);
        return &led;
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
        Ml307Board::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(XINGZHI_CUBE_2_0TFT_4G);
