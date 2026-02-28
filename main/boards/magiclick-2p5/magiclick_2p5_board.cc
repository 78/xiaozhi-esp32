#include "dual_network_board.h"
#include "display/lcd_display.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/circular_strip.h"
#include "config.h"
#include "assets/lang_config.h"

#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include "power_manager.h"
#include "power_save_timer.h"
#include "esp_wifi.h"

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#define TAG "magiclick_2p5"

static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};

class magiclick_2p5 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button main_button_;
    Button left_button_;
    Button right_button_;
    LcdDisplay* display_;

    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;

    uint8_t pcb_version_ = 0;

    // 屏幕配置结构体
    struct DisplayConfig {
        bool use_gc9107;
        bool mirror_x;
        bool mirror_y;
        bool swap_xy;
        bool invert_color;
        lcd_rgb_element_order_t rgb_order;
        int offset_x;
        int offset_y;
        int spi_mode;
        const char* screen_name;
    };

    DisplayConfig GetDisplayConfig() {
        if (pcb_version_ == PCB_VERSION_2_5A) {
            return DisplayConfig{
                .use_gc9107 = true,
                .mirror_x = false,
                .mirror_y = false,
                .swap_xy = false,
                .invert_color = false,
                .rgb_order = LCD_RGB_ELEMENT_ORDER_RGB,
                .offset_x = 0,
                .offset_y = 0,
                .spi_mode = 0,
                .screen_name = "GC9107"
            };
        } else if (pcb_version_ == PCB_VERSION_2_5A1) {
            return DisplayConfig{
                .use_gc9107 = false,
                .mirror_x = true,
                .mirror_y = true,
                .swap_xy = false,
                .invert_color = true,
                .rgb_order = LCD_RGB_ELEMENT_ORDER_BGR,
                .offset_x = 2,
                .offset_y = 3,
                .spi_mode = 0,
                .screen_name = "ST7735"
            };
        } else {
            ESP_LOGW(TAG, "Unknown PCB version: %d, using default ST7735 configuration", pcb_version_);
            return DisplayConfig{
                .use_gc9107 = false,
                .mirror_x = true,
                .mirror_y = true,
                .swap_xy = false,
                .invert_color = true,
                .rgb_order = LCD_RGB_ELEMENT_ORDER_BGR,
                .offset_x = 2,
                .offset_y = 3,
                .spi_mode = 0,
                .screen_name = "ST7735 (default)"

            };
        }
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_48);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(240, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
         
        power_save_timer_->SetEnabled(true);
    }

    void Enable4GModule() {
        // enable the 4G module
        gpio_reset_pin(ML307_POWER_PIN);
        gpio_set_direction(ML307_POWER_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(ML307_POWER_PIN, ML307_POWER_OUTPUT_INVERT ? 0 : 1);
    }
    void Disable4GModule() {
        // enable the 4G module
        gpio_reset_pin(ML307_POWER_PIN);
        gpio_set_direction(ML307_POWER_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(ML307_POWER_PIN, ML307_POWER_OUTPUT_INVERT ? 1 : 0);
    }

    void InitializeCodecI2c() {
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

    void CheckNetType() {
        if (GetNetworkType() == NetworkType::WIFI) {
            Disable4GModule();
        } else if (GetNetworkType() == NetworkType::ML307) {
            Enable4GModule();
        }
        
    }

    //通过adc读取IO3引脚的电压来获取PCB版本
    void CheckPCBVersion() {
        adc_oneshot_unit_handle_t adc1_handle;
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));

        //获取10次， 然后求平均
        int adc_value = 0;
        int raw_value;
        for (int i = 0; i < 10; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &raw_value));
            adc_value += raw_value;
        }
        adc_value /= 10;    
        
        int voltage = adc_value * (3300 / 4095.0);
        if (voltage < 100) {
            // 版本1
            pcb_version_ = PCB_VERSION_2_5A;
        } else if (voltage > 3200 ) {
            // 版本2
            pcb_version_ = PCB_VERSION_2_5A1;   
        }
        // test
        // pcb_version_ = PCB_VERSION_2_5A1;
        
        adc_oneshot_del_unit(adc1_handle);
        ESP_LOGI(TAG, "io voltage: %d, pcb_version: %d\n", voltage, pcb_version_);
    }

    void InitializeButtons() {
        main_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    // cast to WifiBoard
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.EnterWifiConfigMode();
                }
            }
        });        
        main_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });
        main_button_.OnPressDown([this]() {
            power_save_timer_->WakeUp();
            Application::GetInstance().StartListening();
        });
        main_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        left_button_.OnClick([this]() {
            power_save_timer_->WakeUp();            
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        left_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

        right_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        right_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });
    }

    void InitializeLedPower() {
        // 设置GPIO模式
        gpio_reset_pin(BUILTIN_LED_POWER);
        gpio_set_direction(BUILTIN_LED_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level(BUILTIN_LED_POWER, BUILTIN_LED_POWER_OUTPUT_INVERT ? 0 : 1);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay(){
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 获取屏幕配置
        DisplayConfig config = GetDisplayConfig();
        ESP_LOGW(TAG, "PCB Version: %d, Using %s screen", pcb_version_, config.screen_name);

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = config.spi_mode;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");  
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = config.rgb_order;
        panel_config.bits_per_pixel = 16; 
        panel_config.flags.reset_active_high = 0;   

        if (config.use_gc9107) {
            gc9a01_vendor_config_t gc9107_vendor_config = {
                .init_cmds = gc9107_lcd_init_cmds,
                .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
            };
            panel_config.vendor_config = &gc9107_vendor_config;
            ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        } else {
            ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        }

        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, config.invert_color);
        esp_lcd_panel_swap_xy(panel, config.swap_xy);
        esp_lcd_panel_mirror(panel, config.mirror_x, config.mirror_y);

        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, config.offset_x, config.offset_y, config.mirror_x, config.mirror_y, config.swap_xy);
    }

public:
    magiclick_2p5() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC, 0),
        main_button_(MAIN_BUTTON_GPIO),
        left_button_(LEFT_BUTTON_GPIO), 
        right_button_(RIGHT_BUTTON_GPIO) {
        CheckPCBVersion();
        InitializeLedPower();
        CheckNetType();        
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeButtons();
        InitializeSpi();
        InitializeLcdDisplay();
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, BUILTIN_LED_NUM);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
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

DECLARE_BOARD(magiclick_2p5);
