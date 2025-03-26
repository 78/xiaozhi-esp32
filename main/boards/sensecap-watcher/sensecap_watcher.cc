#include "display/lv_display.h"
#include "misc/lv_event.h"
#include "wifi_board.h"
#include "sensecap_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "knob.h"
#include "config.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "power_save_timer.h"

#include <esp_log.h>
#include "esp_check.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <driver/spi_master.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include <iot_button.h>
#include <iot_knob.h>
#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_sleep.h>
#include "esp_console.h"

#define TAG "sensecap_watcher"


LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

class SensecapWatcher : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    LcdDisplay* display_;
    std::unique_ptr<Knob> knob_;
    esp_io_expander_handle_t io_exp_handle;
    button_handle_t btns;
    PowerSaveTimer* power_save_timer_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            bool is_charging = (IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
            if (is_charging) {
                ESP_LOGI(TAG, "charging");
                GetBacklight()->SetBrightness(0);
            } else {
                IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
            }
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = BSP_GENERAL_I2C_SDA,
            .scl_io_num = BSP_GENERAL_I2C_SCL,
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

    esp_err_t IoExpanderSetLevel(uint16_t pin_mask, uint8_t level) {
        return esp_io_expander_set_level(io_exp_handle, pin_mask, level);
    }

    uint8_t IoExpanderGetLevel(uint16_t pin_mask) {
        uint32_t pin_val = 0;
        esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
        pin_mask &= DRV_IO_EXP_INPUT_MASK;
        return (uint8_t)((pin_val & pin_mask) ? 1 : 0);
    }

    void InitializeExpander() {
        esp_err_t ret = ESP_OK;
        esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_001, &io_exp_handle);

        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_INPUT_MASK, IO_EXPANDER_INPUT);
        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, IO_EXPANDER_OUTPUT);
        ret |= esp_io_expander_set_level(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, 0);
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_SYSTEM, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_START_UP, 1);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    
        uint32_t pin_val = 0;
        ret |= esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
        ESP_LOGI(TAG, "IO expander initialized: %x", DRV_IO_EXP_OUTPUT_MASK | (uint16_t)pin_val);
    
        assert(ret == ESP_OK);
    }

    void OnKnobRotate(bool clockwise) {
        auto codec = GetAudioCodec();
        int current_volume = codec->output_volume();
        int new_volume = current_volume + (clockwise ? 5 : -5);

        // 确保音量在有效范围内
        if (new_volume > 100) {
            new_volume = 100;
            ESP_LOGW(TAG, "Volume reached maximum limit: %d", new_volume);
        } else if (new_volume < 0) {
            new_volume = 0;
            ESP_LOGW(TAG, "Volume reached minimum limit: %d", new_volume);
        }

        codec->SetOutputVolume(new_volume);
        ESP_LOGI(TAG, "Volume changed from %d to %d", current_volume, new_volume);
        
        // 显示通知前检查实际变化
        if (new_volume != codec->output_volume()) {
            ESP_LOGE(TAG, "Failed to set volume! Expected:%d Actual:%d", 
                   new_volume, codec->output_volume());
        }
        GetDisplay()->ShowNotification("音量: " + std::to_string(codec->output_volume()));
        power_save_timer_->WakeUp();
    }

    void InitializeKnob() {
        knob_ = std::make_unique<Knob>(BSP_KNOB_A_PIN, BSP_KNOB_B_PIN);
        knob_->OnRotate([this](bool clockwise) {
            ESP_LOGD(TAG, "Knob rotation detected. Clockwise:%s", clockwise ? "true" : "false");
            OnKnobRotate(clockwise);
        });
        ESP_LOGI(TAG, "Knob initialized with pins A:%d B:%d", BSP_KNOB_A_PIN, BSP_KNOB_B_PIN);
    }

    void InitializeButton() {
        button_config_t btn_config = {
            .type = BUTTON_TYPE_CUSTOM,
            .long_press_time = 2000,
            .short_press_time = 50,
            .custom_button_config = {
                .active_level = 0,
                .button_custom_init =nullptr,
                .button_custom_get_key_value = [](void *param) -> uint8_t {
                    auto self = static_cast<SensecapWatcher*>(param);
                    return self->IoExpanderGetLevel(BSP_KNOB_BTN);
                },
                .button_custom_deinit = nullptr,
                .priv = this,
            },
        };
        
        //watcher 是通过长按滚轮进行开机的, 需要等待滚轮释放, 否则用户开机松手时可能会误触成单击
        ESP_LOGI(TAG, "waiting for knob button release");
        while(IoExpanderGetLevel(BSP_KNOB_BTN) == 0) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        btns = iot_button_create(&btn_config);
        iot_button_register_cb(btns, BUTTON_SINGLE_CLICK, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                self->ResetWifiConfiguration();
            }
            self->power_save_timer_->WakeUp();
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(btns, BUTTON_LONG_PRESS_START, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            bool is_charging = (self->IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
            if (is_charging) {
                ESP_LOGI(TAG, "charging");
            } else {
                self->IoExpanderSetLevel(BSP_PWR_LCD, 0);
                self->IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
            }
        }, this);
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");

        spi_bus_config_t qspi_cfg = {0};
        qspi_cfg.sclk_io_num = BSP_SPI3_HOST_PCLK;
        qspi_cfg.data0_io_num = BSP_SPI3_HOST_DATA0;
        qspi_cfg.data1_io_num = BSP_SPI3_HOST_DATA1;
        qspi_cfg.data2_io_num = BSP_SPI3_HOST_DATA2;
        qspi_cfg.data3_io_num = BSP_SPI3_HOST_DATA3;
        qspi_cfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * DRV_LCD_BITS_PER_PIXEL / 8 / CONFIG_BSP_LCD_SPI_DMA_SIZE_DIV;
    
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &qspi_cfg, SPI_DMA_CH_AUTO));
    }

    void Initializespd2010Display() {
        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = BSP_LCD_SPI_CS,
            .dc_gpio_num = -1,
            .spi_mode = 3,
            .pclk_hz = DRV_LCD_PIXEL_CLK_HZ,
            .trans_queue_depth = 2,
            .lcd_cmd_bits = DRV_LCD_CMD_BITS,
            .lcd_param_bits = DRV_LCD_PARAM_BITS,
            .flags = {
                .quad_mode = true,
            },
        };
        spd2010_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &panel_io_);
    
        ESP_LOGD(TAG, "Install LCD driver");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = BSP_LCD_GPIO_RST, // Shared with Touch reset
            .rgb_ele_order = DRV_LCD_RGB_ELEMENT_ORDER,
            .bits_per_pixel = DRV_LCD_BITS_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_spd2010(panel_io_, &panel_config, &panel_);

        esp_lcd_panel_reset(panel_);
        esp_lcd_panel_init(panel_);
        esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel_, true);

        display_ = new SpiLcdDisplay(panel_io_, panel_,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
            {
                .text_font = &font_puhui_30_4,
                .icon_font = &font_awesome_30_4,
                .emoji_font = font_emoji_64_init(),
            });
        
        // 使每次刷新的行数是4的倍数，防止花屏
        lv_display_add_event_cb(lv_display_get_default(), [](lv_event_t *e) {
                lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
                uint16_t x1 = area->x1;
                uint16_t x2 = area->x2;
                // round the start of area down to the nearest 4N number
                area->x1 = (x1 >> 2) << 2;
                // round the start of area down to the nearest 4N number
                area->x1 = (x1 >> 2) << 2;
              
                // round the end of area up to the nearest 4M+3 number
                area->x2 = ((x2 >> 2) << 2) + 3;
                // round the end of area up to the nearest 4M+3 number
                area->x2 = ((x2 >> 2) << 2) + 3;
        }, LV_EVENT_INVALIDATE_AREA, NULL);
        
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }

    uint16_t BatterygetVoltage(void)
    {
        static bool initialized = false;
        static adc_oneshot_unit_handle_t adc_handle;
        static adc_cali_handle_t cali_handle = NULL;
        if (!initialized)
        {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
            };
            adc_oneshot_new_unit(&init_config, &adc_handle);
    
            adc_oneshot_chan_cfg_t ch_config = {
                .atten = BSP_BAT_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            adc_oneshot_config_channel(adc_handle, BSP_BAT_ADC_CHAN, &ch_config);
    
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .chan = BSP_BAT_ADC_CHAN,
                .atten = BSP_BAT_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK)
            {
                initialized = true;
            }
        }
        if (initialized)
        {
            int raw_value = 0;
            int voltage = 0; // mV
            adc_oneshot_read(adc_handle, BSP_BAT_ADC_CHAN, &raw_value);
            adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage);
            voltage = voltage * 82 / 20;
            // ESP_LOGI(TAG, "voltage: %dmV", voltage);
            return (uint16_t)voltage;
        }
        return 0;
    }

    uint8_t BatterygetPercent(void)
    {
        int voltage = 0;
        for (uint8_t i = 0; i < 10; i++)
        {
            voltage += BatterygetVoltage();
        }
        voltage /= 10;
        int percent = (-1 * voltage * voltage + 9016 * voltage - 19189000) / 10000;
        percent = (percent > 100) ? 100 : (percent < 0) ? 0 : percent;
        ESP_LOGI(TAG, "voltage: %dmV, percentage: %d%%", voltage, percent);
        return (uint8_t)percent;
    }

    void InitializeCmd() {
        esp_console_repl_t *repl = NULL;
        esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
        repl_config.max_cmdline_length = 1024;
        repl_config.prompt = "SenseCAP>";
        
        const esp_console_cmd_t cmd1 = {
            .command = "reboot",
            .help = "reboot the device",
            .hint = nullptr,
            .func = [](int argc, char** argv) -> int {
                esp_restart();
                return 0;
            },
            .argtable = nullptr
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd1));

        const esp_console_cmd_t cmd2 = {
            .command = "shutdown",
            .help = "shutdown the device",
            .hint = nullptr,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void *context,int argc, char** argv) -> int {
                auto self = static_cast<SensecapWatcher*>(context);
                self->GetBacklight()->SetBrightness(0);
                self->IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
                return 0;
            },
            .context =this
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd2));


        const esp_console_cmd_t cmd3 = {
            .command = "battery",
            .help = "get battery percent",
            .hint = NULL,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void *context,int argc, char** argv) -> int {
                auto self = static_cast<SensecapWatcher*>(context);
                self->BatterygetPercent();
                return 0;
            },
            .context =this
        };
        ESP_ERROR_CHECK( esp_console_cmd_register(&cmd3) );

        esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
        ESP_ERROR_CHECK(esp_console_start_repl(repl));
    }

public:
    SensecapWatcher(){
        ESP_LOGI(TAG, "Initialize Sensecap Watcher");
        InitializePowerSaveTimer();
        InitializeI2c();
        InitializeSpi();
        InitializeExpander();
        InitializeCmd();
        InitializeButton();
        InitializeKnob();
        Initializespd2010Display();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static SensecapAudioCodec audio_codec(
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
            AUDIO_CODEC_ES7243E_ADDR, 
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

    // 根据 https://github.com/Seeed-Studio/OSHW-SenseCAP-Watcher/blob/main/Hardware/SenseCAP_Watcher_v1.0_SCH.pdf
    // RGB LED型号为 ws2813 mini, 连接在GPIO 40，供电电压 3.3v, 没有连接 BIN 双信号线
    // 可以直接兼容SingleLED采用的ws2812
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(SensecapWatcher);
