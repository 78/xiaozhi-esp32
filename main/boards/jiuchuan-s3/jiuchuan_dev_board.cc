#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "esp_lcd_panel_gc9301.h"

#include "power_save_timer.h"
#include "power_manager.h"
#include "power_controller.h"
#include "gpio_manager.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#define TAG "JiuchuanDevBoard"
#define __USER_GPIO_PWRDOWN__

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class JiuchuanDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button pwr_button_;
    Button wifi_button;
    Button cmd_button;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    
    void InitializePowerManager() {
        power_manager_ = new PowerManager(PWR_ADC_GPIO);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        #ifndef __USER_GPIO_PWRDOWN__
        RTC_DATA_ATTR static bool long_press_occurred = false;
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            ESP_LOGI(TAG, "Wake up by EXT0");
            const int64_t start = esp_timer_get_time();
            ESP_LOGI(TAG, "esp_sleep_get_wakeup_cause");
            while (gpio_get_level(PWR_BUTTON_GPIO) == 0) {
                if (esp_timer_get_time() - start > 3000000) {
                    long_press_occurred = true;
                    break;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            
            if (long_press_occurred) {
                ESP_LOGI(TAG, "Long press wakeup");
                long_press_occurred = false;
            } else {
                ESP_LOGI(TAG, "Short press, return to sleep");
                ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0));
                ESP_ERROR_CHECK(rtc_gpio_pullup_en(PWR_BUTTON_GPIO));  // 内部上拉
                ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(PWR_BUTTON_GPIO));
                esp_deep_sleep_start();
            }
        }
        #endif
        //一分钟进入浅睡眠，5分钟进入深睡眠关机
        power_save_timer_ = new PowerSaveTimer(-1, (60*10), (60*30));
        // power_save_timer_ = new PowerSaveTimer(-1, 6, 10);//test
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
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            #ifndef __USER_GPIO_PWRDOWN__
            ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0));
            ESP_ERROR_CHECK(rtc_gpio_pullup_en(PWR_BUTTON_GPIO));  // 内部上拉
            ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(PWR_BUTTON_GPIO));

            esp_lcd_panel_disp_on_off(panel, false); //关闭显示
            esp_deep_sleep_start();
            #else
            rtc_gpio_set_level(PWR_EN_GPIO, 0);
            rtc_gpio_hold_dis(PWR_EN_GPIO);
            #endif
        });
        power_save_timer_->SetEnabled(true);
    }


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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

    }

    void InitializeButtons() {
        auto& powerCtrl = PowerController::Instance();
        powerCtrl.SetState(PowerController::PowerState::ACTIVE);  // 确保初始状态为ACTIVE
        
        // 配置GPIO
        ESP_LOGI(TAG, "Configuring power button GPIO");
        GpioManager::Config(GPIO_NUM_3, GpioManager::GpioMode::INPUT_PULLDOWN);
        GpioManager::Config(PWR_EN_GPIO, GpioManager::GpioMode::OUTPUT);
        GpioManager::SetLevel(PWR_EN_GPIO, 1);  // 确保电源使能

        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Boot button clicked");
            power_save_timer_->WakeUp();
        });

        // 检查电源按钮初始状态
        ESP_LOGI(TAG, "Power button initial state: %d", GpioManager::GetLevel(PWR_BUTTON_GPIO));
        
        // 高电平有效长按关机逻辑
        pwr_button_.OnLongPress([&powerCtrl]() {
            ESP_LOGI(TAG, "Power button long press detected (high-active)");
            
            // 高电平有效防抖确认
            for (int i = 0; i < 5; i++) {
                int level = GpioManager::GetLevel(PWR_BUTTON_GPIO);
                ESP_LOGD(TAG, "Debounce check %d: GPIO%d level=%d", i+1, PWR_BUTTON_GPIO, level);
                
                if (level == 0) {
                    ESP_LOGW(TAG, "Power button inactive during confirmation - abort shutdown");
                    return;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "Confirmed power button pressed (level=1)");
            
            ESP_LOGI(TAG, "Confirmed power button pressed - initiating shutdown");
            powerCtrl.SetState(PowerController::PowerState::SHUTDOWN);
            
            // 确保状态变更
            if (powerCtrl.GetState() != PowerController::PowerState::SHUTDOWN) {
                ESP_LOGE(TAG, "Failed to set shutdown state!");
            }
        });

        wifi_button.OnClick([this, &powerCtrl]() {
            ESP_LOGI(TAG, "Wifi button clicked");
            power_save_timer_->WakeUp();
            
            if (powerCtrl.GetState() == PowerController::PowerState::ACTIVE) {
                ESP_LOGI(TAG, "Resetting WiFi configuration");
                GpioManager::SetLevel(PWR_EN_GPIO, 1);
                ResetWifiConfiguration();
            }
        });

        cmd_button.OnClick([this]() {
            ESP_LOGI(TAG, "Command button clicked");
            power_save_timer_->WakeUp();
            Application::GetInstance().ToggleChatState();
        });

        // 配置电源状态变更回调（优化版）
        powerCtrl.OnStateChange([this](PowerController::PowerState newState) {
            switch(newState) {
                case PowerController::PowerState::SHUTDOWN: {
                    ESP_LOGI(TAG, "Entering shutdown sequence");
                    
                    // 统一唤醒触发条件
                    #ifndef __USER_GPIO_PWRDOWN__
                    ESP_LOGD(TAG, "Configuring high-level wakeup on GPIO%d", PWR_BUTTON_GPIO);
                    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 1)); // 高电平唤醒
                    #else
                    ESP_LOGD(TAG, "Powering down via GPIO control");
                    GpioManager::SetLevel(PWR_EN_GPIO, 0);
                    #endif
                    
                    // 确保所有外设已关闭
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "Initiating deep sleep");
                    
                    // 最后状态确认(通过单例访问)
                    if (PowerController::Instance().GetState() != PowerController::PowerState::SHUTDOWN) {
                        ESP_LOGE(TAG, "State inconsistency! Forcing shutdown");
                    }
                    esp_deep_sleep_start();
                    break;
                }
                    
                default:
                    ESP_LOGD(TAG, "State changed to %d", static_cast<int>(newState));
                    break;
            }
        });

    }

    void InitializeGC9301isplay() {
        // 液晶屏控制IO初始化
        ESP_LOGI(TAG, "test Install panel IO");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
        // 初始化SPI总线
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io);

        // 初始化液晶屏驱动芯片9309
        ESP_LOGI(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ENDIAN_BGR;
        panel_config.bits_per_pixel = 16;
        esp_lcd_new_panel_gc9309na(panel_io, &panel_config, &panel);

        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
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

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }
public:
    JiuchuanDevBoard() :
    boot_button_(BOOT_BUTTON_GPIO),
    pwr_button_(PWR_BUTTON_GPIO,true),
    wifi_button(WIFI_BUTTON_GPIO),
    cmd_button(CMD_BUTTON_GPIO) { 
        InitializeI2c();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeButtons();
        InitializeGC9301isplay();
        InitializeIot();
        GetBacklight()->RestoreBrightness();

    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {

        static Es8311AudioCodec audio_codec(
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
            AUDIO_CODEC_ES8311_ADDR);
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
};

DECLARE_BOARD(JiuchuanDevBoard);