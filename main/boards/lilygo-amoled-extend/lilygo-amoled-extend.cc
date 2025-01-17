#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include <esp_sleep.h>

#include "display/rm67162_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "encoder.h"

#include "led/single_led.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
// #include "driver/adc.h"
// #include "esp_adc_cal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "bmp280.h"
#include <esp_wifi.h>
#include "rx8900.h"
#include "esp_sntp.h"

#define TAG "LilyGoAmoled"

static rx8900_handle_t _rx8900 = NULL;

class LilyGoAmoled : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button touch_button_;
    Encoder volume_encoder_;
    // SystemReset system_reset_;
    Rm67162Display *display_;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    i2c_bus_handle_t i2c_bus = NULL;
    bmp280_handle_t bmp280 = NULL;
    rx8900_handle_t rx8900 = NULL;

    // esp_adc_cal_characteristics_t adc_chars;

    void InitializeI2c()
    {
        // i2c_master_bus_config_t bus_config = {
        //     .i2c_port = (i2c_port_t)0,
        //     .sda_io_num = IIC_SDA_NUM,
        //     .scl_io_num = IIC_SCL_NUM,
        //     .clk_source = I2C_CLK_SRC_DEFAULT,
        //     .glitch_ignore_cnt = 7,
        //     .intr_priority = 0,
        //     .trans_queue_depth = 0,
        //     .flags = {
        //         .enable_internal_pullup = 1,
        //     },
        // };
        // ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));

        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = IIC_SDA_NUM,
            .scl_io_num = IIC_SCL_NUM,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {0},
            .clk_flags = 0,
        };
        conf.master.clk_speed = 400000,
        i2c_bus = i2c_bus_create(IIC_MASTER_NUM, &conf);
        bmp280 = bmp280_create(i2c_bus, BMP280_I2C_ADDRESS_DEFAULT);
        ESP_LOGI(TAG, "bmp280_default_init:%d", bmp280_default_init(bmp280));
        rx8900 = rx8900_create(i2c_bus, RX8900_I2C_ADDRESS_DEFAULT);
        ESP_LOGI(TAG, "rx8900_default_init:%d", rx8900_default_init(rx8900));
        _rx8900 = rx8900;
        xTaskCreate([](void *arg)
                    {
            sntp_set_time_sync_notification_cb([](struct timeval *t){
                struct tm tm_info;
                localtime_r(&t->tv_sec, &tm_info);
                char time_str[50];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

                ESP_LOGW(TAG, "The net time is: %s", time_str);
                rx8900_write_time(_rx8900, &tm_info);
            });
            esp_netif_init();
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, (char*)NTP_SERVER1);
            esp_sntp_setservername(1, (char*)NTP_SERVER2);
            esp_sntp_init();
            setenv("TZ", DEFAULT_TIMEZONE, 1);
            tzset();
        // configTzTime(DEFAULT_TIMEZONE, NTP_SERVER1, NTP_SERVER2);
        vTaskDelete(NULL); }, "timesync", 4096, NULL, 4, nullptr);
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();           
             if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });

        boot_button_.OnLongPress([this]
                                 {
            ESP_LOGI(TAG, "System Sleeped");
            esp_wifi_stop();
            gpio_set_level(PIN_NUM_LCD_POWER, 0);
            esp_sleep_enable_ext0_wakeup(TOUCH_BUTTON_GPIO, 0);
            esp_deep_sleep_start(); });

        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });
    }
    void InitializeEncoder()
    {
        volume_encoder_.OnPcntReach([this](int value)
                                    {
            static int lastvalue = 0;
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume();
            if(value>lastvalue)
            {
                volume += 4;
                if (volume > 100) {
                    volume = 100;
                }
            }
            else if(value<lastvalue)
            {
                volume -= 4;
                if (volume < 0) {
                    volume = 0;
                }
            }
            lastvalue = value;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });
    }

#define SH8601_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                                    \
        .data0_io_num = d0,                                              \
        .data1_io_num = d1,                                              \
        .sclk_io_num = sclk,                                             \
        .data2_io_num = d2,                                              \
        .data3_io_num = d3,                                              \
        .data4_io_num = GPIO_NUM_NC,                                     \
        .data5_io_num = GPIO_NUM_NC,                                     \
        .data6_io_num = GPIO_NUM_NC,                                     \
        .data7_io_num = GPIO_NUM_NC,                                     \
        .data_io_default_level = 0,                                      \
        .max_transfer_sz = max_trans_sz,                                 \
        .flags = 0,                                                      \
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,                        \
        .intr_flags = 0                                                  \
    }
    void InitializeSpi()
    {
        ESP_LOGI(TAG, "Enable amoled power");
        gpio_set_direction(PIN_NUM_LCD_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_NUM_LCD_POWER, 1);
        ESP_LOGI(TAG, "Initialize SPI bus");
        const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(PIN_NUM_LCD_PCLK,
                                                                     PIN_NUM_LCD_DATA0,
                                                                     PIN_NUM_LCD_DATA1,
                                                                     PIN_NUM_LCD_DATA2,
                                                                     PIN_NUM_LCD_DATA3,
                                                                     DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
    }

    void InitializeRm67162Display()
    {
        display_ = new Rm67162Display(LCD_HOST, (int)PIN_NUM_LCD_CS, (int)PIN_NUM_LCD_RST,
                                      DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Barometer"));
        thing_manager.AddThing(iot::CreateThing("Displayer"));
        // thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

    void InitializeAdc()
    {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc_handle));

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };

        // 创建并初始化校准句柄
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));

        // adc1_config_width(ADC_WIDTH_BIT_12);
        // adc1_config_channel_atten(BAT_DETECT_CH, ADC_ATTEN_DB_12);
        // esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
    }

public:
    LilyGoAmoled() : boot_button_(BOOT_BUTTON_GPIO),
                     touch_button_(TOUCH_BUTTON_GPIO),
                     volume_encoder_(VOLUME_ENCODER1_GPIO, VOLUME_ENCODER2_GPIO)
    // ,
    // system_reset_(RESET_NVS_BUTTON_GPIO, RESET_FACTORY_BUTTON_GPIO)
    {
        // Check if the reset button is pressed
        // system_reset_.CheckButtons();
        InitializeAdc();
        InitializeI2c();
        InitializeSpi();
        InitializeRm67162Display();
        InitializeButtons();
        InitializeEncoder();
        InitializeIot();
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual float GetBarometer() override
    {
        float pressure = 0.0f;
        if (ESP_OK == bmp280_read_pressure(bmp280, &pressure))
        {
            ESP_LOGI(TAG, "pressure:%f ", pressure);
            return pressure;
        }
        return 0;
    }

    virtual float GetTemperature() override
    {
        float temperature = 0.0f;
        if (ESP_OK == bmp280_read_temperature(bmp280, &temperature))
        {
            ESP_LOGI(TAG, "temperature:%f ", temperature);
            return temperature;
        }
        return 0;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        ***static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                           AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                        AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Sdcard *GetSdcard() override
    {
        static Sdcard sd_card(PIN_NUM_SD_CMD, PIN_NUM_SD_CLK, PIN_NUM_SD_D0, PIN_NUM_SD_D1, PIN_NUM_SD_D2, PIN_NUM_SD_D3);
        return &sd_card;
    }

#define VCHARGE 4050
#define V1 3800
#define V2 3500
#define V3 3300
#define V4 3100

    virtual bool GetBatteryLevel(int &level, bool &charging) override
    {
        static int last_level = 0;
        static bool last_charging = false;
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_value));
        int v1 = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_value, &v1));
        v1 *= 2;
        // ESP_LOGI(TAG, "adc_value: %d, v1: %d", adc_value, v1);
        if (v1 >= VCHARGE)
        {
            level = last_level;
            charging = true;
        }
        else if (v1 >= V1)
        {
            level = 100;
            charging = false;
        }
        else if (v1 >= V2)
        {
            level = 75;
            charging = false;
        }
        else if (v1 >= V3)
        {
            level = 50;
            charging = false;
        }
        else if (v1 >= V4)
        {
            level = 25;
            charging = false;
        }
        else
        {
            level = 0;
            charging = false;
        }

        if (level != last_level || charging != last_charging)
        {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        static struct tm time_user;
        rx8900_read_time(rx8900, &time_user);
       ((Rm67162Display*) GetDisplay())->UpdateTime(&time_user);
        
        // char time_str[50];
        // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_user);
        // ESP_LOGI(TAG, "The time is: %s", time_str);
        return true;
    }
};

DECLARE_BOARD(LilyGoAmoled);
