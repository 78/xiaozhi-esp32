#include "dual_network_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "power_manager.h"
#include "power_save_timer.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "esp_private/sdmmc_common.h"
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "customlcddisplay.h"

#define TAG "XINGZHI_ABS_2_0"

typedef enum {
    VIBRATE_EVENT_TRIGGER = 0,
    VIBRATE_EVENT_NONE
} VibrateEvent_t;

class XINGZHI_ABS_2_0 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    // SpiLcdDisplay* display_;
    CustomLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_err_t err;
    bool is_sdcard_found = false;
    QueueHandle_t vibrate_event_queue_;
    TaskHandle_t vibrate_task_handle_;

    void VibrateMotor(uint32_t duration_ms) {
        ESP_LOGI(TAG, "Vibrate Motor");
        gpio_set_level(VIBRATING_MOTOR_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        gpio_set_level(VIBRATING_MOTOR_PIN, 0);
    }

    static void VibrateTask(void* param) {
        XINGZHI_ABS_2_0* board = static_cast<XINGZHI_ABS_2_0*>(param);
        VibrateEvent_t event;
        
        while (1) {
            if (xQueueReceive(board->vibrate_event_queue_, &event, portMAX_DELAY)) {
                if (event == VIBRATE_EVENT_TRIGGER) {
                    board->VibrateMotor(50);
                }
            }
        }
    }

    void TriggerVibrateEvent() {
        VibrateEvent_t event = VIBRATE_EVENT_TRIGGER;
        xQueueSend(vibrate_event_queue_, &event, 0);
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(POWER_USB_IN);//USB是否插入
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
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
            power_manager_->shutdown();
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        for (uint8_t addr = 1; addr < 127; addr++) {
            err = i2c_master_probe(i2c_bus_, addr, 100);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Device found at address 0x%02X", addr);
            }
        }
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

        display_ = new CustomLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        // display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
        //     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeSDcardSpi() {
        spi_bus_config_t bus_cnf = {
            .mosi_io_num = SD_CMD,
            .miso_io_num = SD_DATA0,
            .sclk_io_num = SD_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 400000,
        };

        esp_err_t err = spi_bus_initialize(SD_SPI_HOST, &bus_cnf, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(err));
            return;
        }
        
        static sdspi_device_config_t slot_cnf = {
            .host_id = SD_SPI_HOST,
            .gpio_cs = SD_CS,
            .gpio_cd = SDSPI_SLOT_NO_CD,
            .gpio_wp = GPIO_NUM_NC,
            .gpio_int = GPIO_NUM_NC,
        };
        
        esp_vfs_fat_sdmmc_mount_config_t mount_cnf = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
        };
        
        sdmmc_card_t* card = NULL;
        
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_cnf, &mount_cnf, &card);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SD卡挂载失败: %s", esp_err_to_name(err));
            is_sdcard_found = false;
            return;
        } else if (err == ESP_OK) {
            ESP_LOGI(TAG, "SD卡挂载成功");
            is_sdcard_found = true;
        }
        // sdmmc_card_print_info(stdout, card); // 打印SD卡信息
    }

    void InitializePhysicalButtons() {
        boot_button_.OnClick([this]() {
            this->TriggerVibrateEvent();
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    // cast to WifiBoard
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.EnterWifiConfigMode();
                    return;
                }
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            this->TriggerVibrateEvent();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });

        boot_button_.OnMultipleClick(
            [this]() {
                ESP_LOGI(TAG, "Button OnFiveClick");
                this->TriggerVibrateEvent();
                if (is_sdcard_found) {
                    display_->SetChatMessage("system", "开机检测到SD挂载成功");
                } else {
                    display_->SetChatMessage("system", "开机检测到SD挂载失败");
                }
            }, 5);

        volume_up_button_.OnClick([this]() {
            this->TriggerVibrateEvent();
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            this->TriggerVibrateEvent();
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            this->TriggerVibrateEvent();
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            this->TriggerVibrateEvent();
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeGpio() {
        gpio_config_t zhengdong = {};
        zhengdong.intr_type = GPIO_INTR_DISABLE;
        zhengdong.mode = GPIO_MODE_OUTPUT;
        zhengdong.pin_bit_mask = (1ULL << VIBRATING_MOTOR_PIN);
        zhengdong.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        zhengdong.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&zhengdong);
        gpio_set_level(VIBRATING_MOTOR_PIN, 0);
    }

    void InitializeVibrateTask() {
        vibrate_event_queue_ = xQueueCreate(10, sizeof(VibrateEvent_t));
        if (vibrate_event_queue_ == nullptr) {
            ESP_LOGE(TAG, "创建振动事件队列失败");
            return;
        }

        BaseType_t ret = xTaskCreate(
            VibrateTask,          
            "vibrate_task",       
            2048,                 
            this,                 
            tskIDLE_PRIORITY + 1,
            &vibrate_task_handle_
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "创建振动任务失败");
        } else {
            ESP_LOGI(TAG, "振动任务初始化成功");
        }
    }

public:
    XINGZHI_ABS_2_0() : 
        DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        vibrate_event_queue_(nullptr), 
        vibrate_task_handle_(nullptr) {
        InitializeGpio();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializePhysicalButtons();
        InitializeSDcardSpi();
        InitializeVibrateTask();
        GetBacklight()->RestoreBrightness();
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
            AUDIO_CODEC_I2C_PA_EN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_INPUT_REFERENCE);
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

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(XINGZHI_ABS_2_0);