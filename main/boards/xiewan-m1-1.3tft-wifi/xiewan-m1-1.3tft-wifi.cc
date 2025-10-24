#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_timer.h>

#define TAG "XIEWAN_M1_1_3TFT_WIFI"
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_2
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define BATTERY_READ_INTERVAL_MS (10 * 1000) // 10 秒

class XIEWAN_M1_1_3TFT_WIFI : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay *display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    bool adc_calibrated_;
    esp_timer_handle_t battery_timer_;
    int last_battery_voltage_;
    int last_battery_percentage_;

    // 电池定时器回调函数
    static void BatteryTimerCallback(void *arg)
    {
        static uint8_t timer_count_;
        timer_count_++;
        if (timer_count_ >= 10)
        {
            XIEWAN_M1_1_3TFT_WIFI *board = static_cast<XIEWAN_M1_1_3TFT_WIFI *>(arg);
            board->UpdateBatteryStatus();
            timer_count_ = 0;
        }

        gpio_set_level(BUILTIN_LED_GPIO, timer_count_ % 2);
    }

    // 更新电池状态
    void UpdateBatteryStatus()
    {
        last_battery_voltage_ = ReadBatteryVoltage();
        last_battery_percentage_ = CalculateBatteryPercentage(last_battery_voltage_);

        if (adc_calibrated_)
        {
            // ESP_LOGI(TAG, "电池状态更新 - 电压: %d mV, 电量: %d%%", last_battery_voltage_, last_battery_percentage_);
        }
        else
        {
            ESP_LOGI(TAG, "电池ADC未校准，无法准确读取电压");
        }
    }

    void InitializeCodecI2c()
    {
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

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this](){
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            } 
            app.ToggleChatState(); 
        });
        // volume_up_button_.OnClick([this](){
        //     auto codec = GetAudioCodec();
        //     auto volume = codec->output_volume() + 10;
        //     if (volume > 100) {
        //         volume = 100;
        //     }
        //     codec->SetOutputVolume(volume);
        //     GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); 
        // });

        // volume_up_button_.OnLongPress([this](){
        //     GetAudioCodec()->SetOutputVolume(100);
        //     GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME); 
        // });

        // volume_down_button_.OnClick([this](){
        //     auto codec = GetAudioCodec();
        //     auto volume = codec->output_volume() - 10;
        //     if (volume < 0) {
        //         volume = 0;
        //     }
        //     codec->SetOutputVolume(volume);
        //     GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); 
        // });

        // volume_down_button_.OnLongPress([this](){
        //     GetAudioCodec()->SetOutputVolume(0);
        //     GetDisplay()->ShowNotification(Lang::Strings::MUTED); 
        // });
    }

    void InitializeSt7789Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    // void InitializeIot()
    // {
    //     auto &thing_manager = iot::ThingManager::GetInstance();
    //     thing_manager.AddThing(iot::CreateThing("Speaker"));
    //     thing_manager.AddThing(iot::CreateThing("Screen"));
    //     thing_manager.AddThing(iot::CreateThing("Battery"));
    //     // thing_manager.AddThing(iot::CreateThing("Lamp"));
    // }

    void InitializeBatteryAdc()
    {
        // 初始化 ADC1 单次采样模式
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle_));

        // 配置 ADC 通道
        adc_oneshot_chan_cfg_t config = {
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, BATTERY_ADC_CHANNEL, &config));

        // 初始化校准
        adc_cali_handle_t handle = NULL;
        esp_err_t ret = ESP_FAIL;
        adc_calibrated_ = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        if (!adc_calibrated_)
        {
            ESP_LOGI(TAG, "使用曲线拟合校准方案");
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .chan = BATTERY_ADC_CHANNEL,
                .atten = BATTERY_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
            if (ret == ESP_OK)
            {
                adc_calibrated_ = true;
            }
        }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        if (!adc_calibrated_)
        {
            ESP_LOGI(TAG, "使用线性拟合校准方案");
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = BATTERY_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
            if (ret == ESP_OK)
            {
                adc_calibrated_ = true;
            }
        }
#endif

        adc1_cali_handle_ = handle;
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "ADC校准成功");
        }
        else if (ret == ESP_ERR_NOT_SUPPORTED || !adc_calibrated_)
        {
            ESP_LOGW(TAG, "eFuse未烧录，跳过软件校准");
        }
        else
        {
            ESP_LOGE(TAG, "无效参数或内存不足");
        }
    }

    void InitializeTimer()
    {
        esp_timer_create_args_t timer_args = {
            .callback = &BatteryTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_timer",
            .skip_unhandled_events = false,
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(battery_timer_, 1000 * 1000)); // 微秒为单位
    }

    // 读取电池电压(mV)
    int ReadBatteryVoltage()
    {
        int adc_raw = 0;
        long long sum = 0;
        int voltage = 0;
        for (int i = 0; i < 10; i++)
        {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle_, BATTERY_ADC_CHANNEL, &adc_raw));
            sum += adc_raw;
        }
        adc_raw = sum / 10;

        // ESP_LOGI(TAG, "电池ADC原始数据: %d", adc_raw);

        if (adc_calibrated_)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle_, adc_raw, &voltage));
        }

        // 分压电阻1:2，所以乘以2
        return voltage * 3;
    }

    // 计算电池电量百分比
    int CalculateBatteryPercentage(int voltage)
    {
        if (!adc_calibrated_ || voltage <= 0)
        {
            return -1; // 返回-1表示无法读取
        }

        // 假设电池电压范围为3.0V-4.2V
        const int min_voltage = 3000; // 3.0V
        const int max_voltage = 4200; // 4.2V

        if (voltage < min_voltage)
        {
            return 0;
        }
        else if (voltage > max_voltage)
        {
            return 100;
        }
        else
        {
            return (voltage - min_voltage) * 100 / (max_voltage - min_voltage);
        }
    }

public:
    XIEWAN_M1_1_3TFT_WIFI() : boot_button_(BOOT_BUTTON_GPIO),
                              volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                              volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
                              last_battery_voltage_(0), last_battery_percentage_(-1)
    {
        ESP_LOGI(TAG, "Initializing Gezipai Board");
        // init gpio led
        gpio_config_t led_config = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&led_config));
        InitializeCodecI2c();
        InitializeSpi();
        InitializeButtons();
        InitializeSt7789Display();
        InitializeBatteryAdc();
        // 初始读取一次电量
        UpdateBatteryStatus();
        // 启动定时器定
        InitializeTimer();
        GetBacklight()->RestoreBrightness();
    }

    ~XIEWAN_M1_1_3TFT_WIFI()
    {
        // 停止定时器
        esp_timer_stop(battery_timer_);
        esp_timer_delete(battery_timer_);

        // 释放ADC资源
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle_));

        if (adc_calibrated_)
        {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle_));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(adc1_cali_handle_));
#endif
        }
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // 获取最后一次读取的电池电压(mV)
    int GetBatteryVoltage()
    {
        return last_battery_voltage_;
    }

    // 获取最后一次计算的电池电量百分比
    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging)
    {
        level = last_battery_percentage_;

        uint8_t chargeLevel = gpio_get_level(CHARGING_GPIO);
        uint8_t doneLevel = gpio_get_level(DONE_GPIO);
        // ESP_LOGI(TAG, "读取到的充电状态 - chargeLevel: %d , doneLevel: %d", chargeLevel, doneLevel);
        if (chargeLevel == 0 && doneLevel == 0)
        {
            charging = false;
            discharging = true;
        }
        else if (chargeLevel == 0 && doneLevel == 1)
        {
            charging = true;
            discharging = false;
        }
        else if (chargeLevel == 1 && doneLevel == 0)
        {
            charging = false;
            discharging = true;
        }

        return true;
    }
};

DECLARE_BOARD(XIEWAN_M1_1_3TFT_WIFI);
