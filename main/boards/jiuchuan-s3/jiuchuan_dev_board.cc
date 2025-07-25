#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#ifdef CONFIG_ENABLE_IOT
#include "iot/thing_manager.h"
#endif
#include <nvs_flash.h>
#include <nvs.h>
#include "esp_wifi.h"

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

#undef TAG  // 取消之前的定义
#define TAG "JiuchuanDevBoard"  // 重新定义
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
    int current_volume = 80; // 默认音量值设为80
    static const int VOLUME_STEP = 8; // 音量调整步长

    // 音量映射函数
    int MapVolumeForDisplay(int internal_volume) {
        if (internal_volume < 0) internal_volume = 0;
        if (internal_volume > 80) internal_volume = 80;
        return (internal_volume * 100) / 80;
    }

    // 保存音量到NVS
    void SaveVolumeToNVS(int volume) {
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
            return;
        }
        
        err = nvs_set_i32(nvs_handle, "volume", volume);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error writing volume to NVS: %s", esp_err_to_name(err));
        }
        
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
        }
        
        nvs_close(nvs_handle);
    }
    // 从NVS加载音量
    int LoadVolumeFromNVS() {
        #ifdef CONFIG_USE_NVS
        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "NVS不存在，使用默认音量");
            return current_volume;
        }
        
        int32_t volume = current_volume;
        err = nvs_get_i32(nvs_handle, "volume", &volume);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "读取音量失败: %s", esp_err_to_name(err));
        }
        
        nvs_close(nvs_handle);
        return volume;
        #else
        ESP_LOGI(TAG, "NVS功能未启用，使用默认音量");
        return current_volume;
        #endif
    }
    
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
        power_save_timer_ = new PowerSaveTimer(-1, (60*10), -1); // 取消5分钟深睡眠关机
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
        static bool pwrbutton_unreleased = false;
        static int power_button_click_count = 0;
        static int64_t last_power_button_press_time = 0;

        if (gpio_get_level(GPIO_NUM_3) == 1) {
            pwrbutton_unreleased = true;
        }
        
        ESP_LOGI(TAG, "Configuring power button GPIO");
        GpioManager::Config(GPIO_NUM_3, GpioManager::GpioMode::INPUT_PULLDOWN);

        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Boot button clicked");
            power_save_timer_->WakeUp();
        });

        pwr_button_.OnPressUp([this]() {
            ESP_LOGI(TAG, "电源按钮按下");
            pwrbutton_unreleased = false;
            int64_t current_time = esp_timer_get_time();
            if (current_time - last_power_button_press_time < 400000) { // 400ms内算连续点击
                power_button_click_count++;
                
                if (power_button_click_count >= 3) {
                    ESP_LOGI(TAG, "三击重置WiFi");
                    rtc_gpio_set_level(PWR_EN_GPIO, 1);
                    rtc_gpio_hold_en(PWR_EN_GPIO);
                    ResetWifiConfiguration();
                    power_button_click_count = 0;
                    return;
                }
            } else {
                power_button_click_count = 1;
            }
            
            last_power_button_press_time = current_time;
            auto &app = Application::GetInstance();
            auto current_state = app.GetDeviceState();
            
            if (current_state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "从待命状态切换到聆听状态");
                app.ToggleChatState();
            } else if (current_state == kDeviceStateListening) {
                ESP_LOGI(TAG, "从聆听状态切换到待命状态");
                app.ToggleChatState();
            } else if (current_state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "从说话状态切换到待命状态");
                app.ToggleChatState();
            } else {
                ESP_LOGI(TAG, "唤醒设备");
                power_save_timer_->WakeUp();
            }
        });

        pwr_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "电源键长按");
            if (pwrbutton_unreleased){
                ESP_LOGI(TAG, "开机后电源键未松开,取消关机");
                return;
            }
            
            for (int i = 0; i < 5; i++) {
                int level = GpioManager::GetLevel(PWR_BUTTON_GPIO);
                ESP_LOGD(TAG, "Debounce check %d: GPIO%d level=%d", i+1, PWR_BUTTON_GPIO, level);
                
                if (level == 0) {
                    ESP_LOGW(TAG, "取消关机");
                    return;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            
            ESP_LOGI(TAG, "Confirmed power button pressed (level=1)");
            power_manager_->SetPowerState(PowerState::SHUTDOWN);
        });

        wifi_button.OnPressDown([this]() {
            ESP_LOGI(TAG, "音量增加按键");
            
            current_volume = (current_volume + VOLUME_STEP > 80) ? 80 : current_volume + VOLUME_STEP;
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(current_volume);
            ESP_LOGI(TAG, "当前音量: %d", current_volume);
            SaveVolumeToNVS(current_volume);
            power_save_timer_->WakeUp();
            
            auto display = GetDisplay();
            if (display) {
                int display_volume = MapVolumeForDisplay(current_volume);
                char volume_text[20];
                snprintf(volume_text, sizeof(volume_text), "音量: %d%%", display_volume);
                display->ShowNotification(volume_text);
            }
        });

        cmd_button.OnPressDown([this]() {
            ESP_LOGI(TAG, "音量减少键");
            
            current_volume = (current_volume - VOLUME_STEP < 0) ? 0 : current_volume - VOLUME_STEP;
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(current_volume);
            ESP_LOGI(TAG, "当前音量: %d", current_volume);
            SaveVolumeToNVS(current_volume);
            power_save_timer_->WakeUp();
            
            auto display = GetDisplay();
            if (display) {
                int display_volume = MapVolumeForDisplay(current_volume);
                char volume_text[20];
                snprintf(volume_text, sizeof(volume_text), "音量: %d%%", display_volume);
                display->ShowNotification(volume_text);
            }
        });
    }

    // 物联网初始化
    void InitializeIot() {
        #ifdef CONFIG_ENABLE_IOT
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
        #endif
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

public:
    JiuchuanDevBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        pwr_button_(PWR_BUTTON_GPIO,true),
        wifi_button(WIFI_BUTTON_GPIO),
        cmd_button(CMD_BUTTON_GPIO) { 

        // 初始化NVS
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGI(TAG, "NVS分区已满或版本不匹配，擦除并重新初始化");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);

        // 从NVS加载保存的音量
        current_volume = LoadVolumeFromNVS();
        ESP_LOGI(TAG, "从NVS加载音量: %d", current_volume);

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
