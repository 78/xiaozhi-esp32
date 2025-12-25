#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_io_expander_tca9554.h>
#include <esp_lcd_ili9341.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "esp32_camera.h"
#include "power_manager.h"
#include "power_save_timer.h"

#define TAG "esp32s3_korvo2_v3"
/* ADC Buttons */
typedef enum {
    BSP_ADC_BUTTON_REC,
    BSP_ADC_BUTTON_VOL_MUTE,
    BSP_ADC_BUTTON_PLAY,
    BSP_ADC_BUTTON_SET,
    BSP_ADC_BUTTON_VOL_DOWN,
    BSP_ADC_BUTTON_VOL_UP,
    BSP_ADC_BUTTON_NUM
} bsp_adc_button_t;

// Init ili9341 by custom cmd
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},
    {0xC5, (uint8_t []){0xD0}, 1, 0},
    {0xC1, (uint8_t []){0x02}, 1, 0},
    {0xB4, (uint8_t []){0x02}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},

    {0xB1, (uint8_t []){00, 0x1B}, 2, 0},
    {0x36, (uint8_t []){0x08}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xB7, (uint8_t []){0x06}, 1, 0},

    {0x11, (uint8_t []){0}, 0x80, 0},
    {0x29, (uint8_t []){0}, 0x80, 0},

    {0, (uint8_t []){0}, 0xff, 0},
};

class Esp32S3Korvo2V3Board : public WifiBoard {
private:
    Button boot_button_;
    Button* adc_button_[BSP_ADC_BUTTON_NUM];
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    adc_oneshot_unit_handle_t bsp_adc_handle = NULL;
#endif
    i2c_master_bus_handle_t i2c_bus_;
    LcdDisplay* display_;
    esp_io_expander_handle_t io_expander_ = NULL;
    Esp32Camera* camera_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    void InitializePowerManager() {
        // PowerManager需要复用按钮的ADC句柄，所以在InitializeButtons之后调用
        // 传入按钮的ADC句柄指针，让PowerManager复用
        power_manager_ = new PowerManager(GPIO_NUM_NC, &bsp_adc_handle);
    }
    
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
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

    void InitializeTca9554() {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander_);
        if(ret != ESP_OK) {
            ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000, &io_expander_);
            if(ret != ESP_OK) {
                ESP_LOGE(TAG, "TCA9554 create returned error");  
                return;
            }
        }
        // 配置IO0-IO3为输出模式
        ESP_ERROR_CHECK(esp_io_expander_set_dir(io_expander_, 
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | 
            IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, 
            IO_EXPANDER_OUTPUT));

        // 复位LCD和TouchPad
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 0));
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));
    }

    void EnableLcdCs() {
        if(io_expander_ != NULL) {
            esp_io_expander_set_level(io_expander_, IO_EXPANDER_PIN_NUM_3, 0);// 置低 LCD CS
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_0;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_1;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void ChangeVol(int val) {
        auto codec = GetAudioCodec();
        auto volume = codec->output_volume() + val;
        if (volume > 100) {
            volume = 100;
        }
        if (volume < 0) {
            volume = 0;
        }
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void MuteVol() {
        auto codec = GetAudioCodec();
        auto volume = codec->output_volume();
        if (volume > 1) {
            volume = 0;
        } else  {
            volume = 50;
        }
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void InitializeButtons() {
         button_adc_config_t adc_cfg = {};
        adc_cfg.adc_channel = ADC_CHANNEL_4; // ADC1 channel 0 is GPIO5
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        const adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        adc_oneshot_new_unit(&init_config1, &bsp_adc_handle);
        adc_cfg.adc_handle = &bsp_adc_handle;
#endif
        adc_cfg.button_index = BSP_ADC_BUTTON_REC;
        adc_cfg.min = 2310; // middle is 2410mV
        adc_cfg.max = 2510;
        adc_button_[0] = new AdcButton(adc_cfg);

        adc_cfg.button_index = BSP_ADC_BUTTON_VOL_MUTE;
        adc_cfg.min = 1880; // middle is 1980mV
        adc_cfg.max = 2080;
        adc_button_[1] = new AdcButton(adc_cfg);

        adc_cfg.button_index = BSP_ADC_BUTTON_PLAY;
        adc_cfg.min = 1550; // middle is 1650mV
        adc_cfg.max = 1750;
        adc_button_[2] = new AdcButton(adc_cfg);

        adc_cfg.button_index = BSP_ADC_BUTTON_SET;
        adc_cfg.min = 1015; // middle is 1115mV
        adc_cfg.max = 1215;
        adc_button_[3] = new AdcButton(adc_cfg);

        adc_cfg.button_index = BSP_ADC_BUTTON_VOL_DOWN;
        adc_cfg.min = 720; // middle is 820mV
        adc_cfg.max = 920;
        adc_button_[4] = new AdcButton(adc_cfg);

        adc_cfg.button_index = BSP_ADC_BUTTON_VOL_UP;
        adc_cfg.min = 280; // middle is 380mV
        adc_cfg.max = 480;
        adc_button_[5] = new AdcButton(adc_cfg);

        auto volume_up_button = adc_button_[BSP_ADC_BUTTON_VOL_UP];
        volume_up_button->OnClick([this]() {ChangeVol(10);});
        volume_up_button->OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        auto volume_down_button = adc_button_[BSP_ADC_BUTTON_VOL_DOWN];
        volume_down_button->OnClick([this]() {ChangeVol(-10);});
        volume_down_button->OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

        auto volume_mute_button = adc_button_[BSP_ADC_BUTTON_VOL_MUTE];
        volume_mute_button->OnClick([this]() {MuteVol();});

        auto play_button = adc_button_[BSP_ADC_BUTTON_PLAY];
        play_button->OnClick([this]() {
             ESP_LOGI(TAG, " TODO %s:%d\n", __func__, __LINE__);
        });

        auto set_button = adc_button_[BSP_ADC_BUTTON_SET];
        set_button->OnClick([this]() {
            EnterWifiConfigMode();
        });

        auto rec_button = adc_button_[BSP_ADC_BUTTON_REC];
        rec_button->OnClick([this]() {
             Application::GetInstance().ToggleChatState();
        });
        boot_button_.OnClick([this]() {});
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.dc_gpio_num = GPIO_NUM_2;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const ili9341_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        // panel_config.flags.reset_active_high = 0,
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        EnableLcdCs();
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_46;
        io_config.dc_gpio_num = GPIO_NUM_2;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        EnableLcdCs();
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCamera() {
        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0,
                [1] = CAMERA_PIN_D1,
                [2] = CAMERA_PIN_D2,
                [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4,
                [5] = CAMERA_PIN_D5,
                [6] = CAMERA_PIN_D6,
                [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 100000,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = XCLK_FREQ_HZ,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new Esp32Camera(video_config);
    }

public:
    Esp32S3Korvo2V3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "Initializing esp32s3_korvo2_v3 Board");
        InitializePowerSaveTimer();
        InitializeI2c();
        I2cDetect();
        InitializeTca9554();
        InitializeCamera();
        InitializeSpi();
        InitializeButtons();  // 先初始化按钮（创建ADC1句柄）
        InitializePowerManager();  // 后初始化PowerManager（复用ADC1句柄）
        #ifdef LCD_TYPE_ILI9341_SERIAL
        InitializeIli9341Display(); 
        #else
        InitializeSt7789Display(); 
        #endif
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
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

    virtual Display *GetDisplay() override {
        return display_;
    }
    virtual Camera* GetCamera() override {
        return camera_;
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
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(Esp32S3Korvo2V3Board);
